# UE 风格反射系统 — 技术设计（方案 B）

## 上下文

**当前状态：**

- `UClass` 仅含类名、父类指针、`CreateObject` 工厂；`UObject::GetClass()` / `IsA()` 已用于 Spawn、序列化类型判别
- `DetailPanelWidget` 硬编码 Transform SpinBox 与调试 CheckBox
- `ComponentSerializer` 用 `if (ComponentClass == "UStaticMeshComponent")` 分支 + 手写 `SerializeSceneComponent` / `SerializeMeshComponent`
- 各资产类（`UStaticMesh`、`UTexture2D` 等）各自实现 `Serialize`/`Deserialize`

**目标：** 参照 UE5 反射架构，用宏标记 + 代码生成 + 运行时元数据表，让编辑器与序列化通过统一反射 API 访问对象成员。

**方案 B 定义：**

- **libclang**：解析完整 C++ AST，提取类继承、字段名/类型、方法签名、内存偏移、枚举项
- **宏参数 Parser**：从源文件文本定位 `UPROPERTY(...)` / `UFUNCTION(...)` / `UCLASS(...)` 调用，用独立 Lexer 解析 `EditAnywhere`、`Category="Transform"`、`meta=(ClampMin=0)` 等参数
- 两者通过 **源码位置（文件 + 行号）** 关联：libclang 给出 `FieldDecl` 行号，宏 Parser 向上扫描最近的前置 `UPROPERTY` 行

**约束：**

- C++20、现有 UE 风格命名与 Allman brace 规范
- libclang **仅用于构建期离线工具**（`ReflectionGenerator`），不链接进 `OpenSpecTest` 游戏/编辑器目标
- libclang 以 **vendoring 预编译包** 提供：`OpenSpecTest/thirdparty/libclang/`（头文件 + import lib + DLL），禁止依赖本机 LLVM 安装或 `find_package(Clang)`
- 生成物风格对齐 UE：`.generated.h`（宏展开/声明）+ `.gen.cpp`（元数据注册）
- Windows 为主开发平台；CMake 集成

## 目标 / 非目标

**目标：**

- 开发者用 `UPROPERTY`/`UFUNCTION` 标记成员后，构建自动生成反射注册代码
- 运行时可通过 `UClass::FindPropertyByName`、`FProperty::GetValue`/`SetValue` 读写
- Details 面板自动展示 `EditAnywhere` 属性（至少支持 `bool`、`int32`、`float`、`double`、`FVector`、枚举、`std::string`）
- `SaveGame` 属性自动参与 `ComponentSerializer` JSON 读写
- 试点覆盖 `USceneComponent` 及其子类

**非目标：**

- 蓝图 / 脚本 VM / `ProcessEvent` 完整派发
- 网络 `Replicated` 属性同步
- GC 基于反射的引用扫描
- 模板类 `UCLASS`、`TSubclassOf`、`TObjectPtr` 等高级类型
- 全量迁移所有 `Serialize()` 手写逻辑（资产类本期保持现状）
- 属性变更撤销/Redo 系统

## 决策

### 决策 1：方案 B — libclang 结构 + 宏参数 Parser 语义

- **选择**：双通道解析，位置关联
- **原因**：
  - libclang 擅长继承链、类型、offset，避免自研 C++ parser
  - UE 宏参数（`Category="X"`、`meta=(...)`）不是 C++ 标准语法，独立 Parser 更可控，且与 UE UHT 思路一致
  - 不采用方案 A（`annotate` 属性化），保持源码写法与 UE 一致，降低迁移心智负担
- **备选**：
  - 方案 A `[[reflect(...)]]` — 破坏 UE 风格宏外观
  - 纯自研 parser（UHT 路线）— 维护成本极高
  - rttr 注册式 — 无代码生成，类增多后不可维护

**关联算法（字段 ↔ 宏）：**

```
1. libclang 遍历 FieldDecl → 记录 (file, line, name, type, offset)
2. 宏 Parser 扫描文件 → 记录 (line, macroType, args) 其中 macroType ∈ {UPROPERTY, UFUNCTION, UCLASS}
3. 对每个 FieldDecl，在同行或上一非空行查找 UPROPERTY；若无则默认无标志
4. 对 CXXMethod，同理关联 UFUNCTION
5. 对 ClassDecl 起始行关联 UCLASS
```

### 决策 2：反射宏设计 — 空宏 + GENERATED_BODY

- **选择**：编译期宏为空操作（或仅生成唯一行标记），真实逻辑全在生成代码

```cpp
// ReflectionMacros.h（示意）
#define UCLASS(...)
#define USTRUCT(...)
#define UENUM(...)
#define UPROPERTY(...)
#define UFUNCTION(...)
#define GENERATED_BODY() /* 由 .generated.h 替换为 FID_xxx_GENERATED_BODY */
```

- **原因**：宏不参与编译语义，避免与 C++ 语法冲突；UHT 同款模式
- **`.generated.h` 内容**：`DECLARE_CLASS`、`StaticRegisterNatives`、`DECLARE_FUNCTION(exec*)` 声明
- **`.gen.cpp` 内容**：`ConstructUClass`、属性描述符数组、`STRUCT_OFFSET`、`DEFINE_FUNCTION` 跳板、静态注册

### 决策 3：运行时类型层次

```
UObject
  └── 元数据（非 UObject 实例，纯 C++ 描述符）
        FField（基类）
          ├── FProperty（bool/int/float/double/string/struct/enum/object）
          ├── UFunction（名称、标志、原生函数指针）
        UClass（扩展现有 UClass：属性链表、函数链表、StaticClass 返回单例）
        UEnum（名称 → 值映射）
        UScriptStruct（USTRUCT 描述，如 FVector）
```

- **选择**：`FProperty` 使用 **offset + 类型特化读写函数指针**，不用 `std::any` 热路径
- **扩展现有 `UClass`**：新增 `FindPropertyByName`、`ForEachProperty`、`GetProperties()`；保留现有 `CreateObject`/`IsChildOf`
- **`FReflectionRegistry`**：全局 `std::unordered_map<std::string, const UClass*>` + `RegisterClass` 静态初始化

### 决策 4：ReflectionGenerator 工具架构

```
Tools/ReflectionGenerator/
  ├── Main.cpp                 # CLI 入口
  ├── ClangAstScanner.cpp      # libclang：类/字段/方法/枚举/offset
  ├── MacroLexer.cpp           # 源文件字符级 lexer
  ├── MacroParser.cpp          # UPROPERTY/UFUNCTION/UCLASS 参数 AST
  ├── MacroAssociator.cpp      # 行号关联
  ├── CodeEmitter.cpp          # 输出 .generated.h / .gen.cpp
  └── CMakeLists.txt           # 链接 vendored libclang import lib
```

**vendored libclang 目录约定（与现有 thirdparty 布局一致）：**

```
OpenSpecTest/thirdparty/libclang/
  include/          # Index.h 等 C API 头文件
  lib/
    Debug/          # libclang.lib（或等价 import lib）
    Release/
  bin/
    Debug/          # libclang.dll + 依赖 DLL（如有）
    Release/
```

`OpenSpecTest/CMakeLists.txt` 已显式 `continue()` 跳过 libclang 的自动 DLL 链接，避免反射工具依赖污染主程序。

**CLI 接口：**

```bash
ReflectionGenerator \
  --compile-commands <path> \
  --scan-dir OpenSpecTest/src \
  --output-dir OpenSpecTest/Generated/Reflection \
  --module OpenSpecTest
```

**扫描规则：**

- 仅处理含 `#include "xxx.generated.h"` 或 `GENERATED_BODY()` 的头文件
- 含 `UCLASS`/`USTRUCT` 标记的声明纳入生成
- 解析失败 → 非零退出码，阻断构建

### 决策 5：libclang 以 vendoring 预编译包引入

- **选择**：将固定 LLVM 版本的 libclang 头文件、import lib、`libclang.dll` 放入 `OpenSpecTest/thirdparty/libclang/`，纳入版本控制（或大文件 LFS）；仅 `ReflectionGenerator` 目标链接并加载
- **原因**：
  - 消除「每台机器安装 LLVM / 配置 `LLVM_DIR`」的环境差异
  - 与项目现有 `thirdparty/assimp`、`thirdparty/json` 管理方式一致
  - 主程序 `OpenSpecTest` 不链接 libclang，避免 DLL 进入编辑器运行目录
- **备选**：
  - `find_package(Clang)` — 已否决，环境不可复现
  - 静态链接 libclang — Windows 预编译包难获取、体积大

**运行时加载**：`ReflectionGenerator` 构建后须将 `libclang.dll` 复制到生成器可执行文件同目录（`POST_BUILD` 或 `add_custom_command` 前设置 `PATH`），否则 AST 解析启动失败。

### 决策 6：CMake 构建集成

- **选择**：
  1. 顶层 CMake 开启 `CMAKE_EXPORT_COMPILE_COMMANDS ON`
  2. `Tools/ReflectionGenerator/CMakeLists.txt` 通过 `OPEN_SPEC_TEST_THIRDPARTY_ROOT` 定位 vendored libclang include/lib
  3. `add_custom_command` 在编译 `OpenSpecTest` 前运行 `ReflectionGenerator`（确保 DLL 可加载）
  4. 生成目录 `OpenSpecTest/Generated/Reflection/` 加入 include path
  5. `.gen.cpp` 编入 `OpenSpecTest` target
- **增量**：输入文件 hash 清单（`.reflection-cache.json`），仅变更头文件时重新生成对应类
- **备选**：每次全量生成 — 简单但慢；首期可全量，后续加缓存

### 决策 7：属性标志位（首期子集）

| 标志 | 用途 |
|------|------|
| `EditAnywhere` | Details 面板显示并可编辑 |
| `VisibleAnywhere` | Details 只读显示 |
| `SaveGame` | 组件/关卡 JSON 序列化 |
| `Transient` | 显式排除序列化 |
| `Category="X"` | Details 分组 |
| `meta=(DisplayName="Y")` | 显示名 |

首期 Parser 支持：标识符标志、`Category="..."` 键值、简单 `meta=(Key=Value)`。

### 决策 8：Details 面板改造

- **选择**：`FPropertyDetailsBuilder` 遍历选中 `UObject` 的 `UClass` 属性链
  - `EditAnywhere` + 非 `Transient` → 创建控件
  - 按 `Category` 分 `QGroupBox`
  - 类型 → 控件映射：`float`/`double`→`DraggableDoubleSpinBox`，`bool`→`QCheckBox`，`FVector`→3×SpinBox，`enum`→`QComboBox`
- **Transform 硬编码 UI**：迁移到 `USceneComponent` 的 `UPROPERTY` 后删除
- **保留**：Actor 级调试 CheckBox 等非反射开关可暂留

### 决策 9：序列化迁移策略

- **选择**：`FPropertySerializer` 遍历 `SaveGame` 属性读写 JSON
- **试点**：`ComponentSerializer` 中 `SerializeSceneComponent`/`SerializeMeshComponent` 改为反射驱动；类名分支保留（用于 `NewObject` 类型选择），字段读写交给反射
- **JSON 键**：默认用属性名；嵌套 `USTRUCT` 用 JSON 对象
- **兼容**：旧存档字段名不变（`Location`、`Rotation`、`Scale`、`MeshAssetPath` 等与现有一致）

## 风险 / 权衡

| 风险 | 缓解 |
|------|------|
| vendored libclang 与 MSVC 工具链不匹配 | 固定 LLVM 版本并文档化；升级时整包替换 `thirdparty/libclang/` |
| `libclang.dll` 未复制到生成器输出目录 | `ReflectionGenerator` POST_BUILD 复制；`add_custom_command` 使用生成器绝对路径 |
| thirdparty 二进制体积增大 | 仅构建期工具使用；不进 `OpenSpecTest` 运行时；可选 Git LFS |
| 宏 Parser 遇复杂 `meta=` 嵌套失败 | 首期限制 meta 语法；解析失败报明确行号错误 |
| 字段 offset 与 packing 不一致 | 使用 libclang `clang_Type_getOffsetOf`；生成期 static_assert 校验 |
| 生成器变慢 | 首期全量可接受；后续 `.reflection-cache.json` 增量 |
| 双轨序列化（手写 + 反射）短期共存 | 试点类迁移完成前文档标注；禁止新手写组件字段序列化 |
| `UPROPERTY` 在字段上方多行注释干扰关联 | Associator 向上扫描跳过注释/空行，最多 N 行 |

## 迁移计划

1. **Phase A**：落地 `reflection-runtime` + 宏头文件 + 手工注册一个试验类验证 API
2. **Phase B**：实现 `ReflectionGenerator`（libclang + 宏 Parser），生成 `USceneComponent`
3. **Phase C**：Details 面板 + `ComponentSerializer` 切到反射
4. **Phase D**：扩展到 `UMeshComponent`、`UStaticMeshComponent`、`USkeletalMeshComponent`
5. **回滚**：关闭 CMake 生成步骤，恢复手写 `StaticClass` 注册（生成物可入库或 `.gitignore` 二选一，首期建议入库以降低构建门槛）

## 待决问题

- ~~libclang 目标 LLVM 版本~~ → **已决**：vendoring 固定版本（`thirdparty/libclang/VERSION` 记录为 LLVM 18.1.8）
- vendored 包是否走 Git LFS（DLL 较大时建议 LFS，头文件与 import lib 可直提）
- ~~生成物是否入库~~ → **已决**：首期 **入库** `Generated/Reflection/`，MSVC 无 `compile_commands.json` 时仍可编译

## 已知限制（实现后）

- **模板 `UCLASS`**：未支持
- **`UObject*` 属性**：未支持；对象引用需下期
- **`Replicated` / 网络复制**：未实现
- **`UFUNCTION` 完整 `exec` 派发**：仅描述符与 `Invoke` 基础设施，无蓝图 VM
- **MSVC + VS 生成器**：默认不产生 `compile_commands.json`，自动 codegen 步骤跳过；需 Ninja/Clang 工具链或手动运行 `ReflectionGenerator`
- **试点范围**：仅 `USceneComponent` 完成反射迁移；`UMeshComponent` 序列化/Details 仍走手写路径
- **Details 面板**：通过 Root `USceneComponent` 反射编辑 Transform；Actor 级 `FActorTransform` 与组件 Relative 同步依赖 `ApplyActorTransformToRoot`
