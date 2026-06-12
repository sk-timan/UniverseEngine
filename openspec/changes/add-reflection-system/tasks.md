## 1. 反射宏与运行时基础

- [x] 1.1 创建 `OpenSpecTest/src/reflection/` 目录与 `ReflectionMacros.h`（`UCLASS`/`USTRUCT`/`UENUM`/`UPROPERTY`/`UFUNCTION`/`GENERATED_BODY` 空宏）
- [x] 1.2 定义 `EPropertyFlags`、`EFunctionFlags`、`FPropertyMetadata` 与 `FField`/`FProperty` 基类层次
- [x] 1.3 实现基础属性类型：`FBoolProperty`、`FIntProperty`、`FFloatProperty`、`FDoubleProperty`、`FStrProperty`
- [x] 1.4 实现 `UScriptStruct` 与 `FStructProperty`（首期支持 `FVector`）
- [x] 1.5 实现 `UEnum` 与 `FEnumProperty`（名称↔值查询）
- [x] 1.6 实现 `UFunction` 描述符与原生函数指针绑定（`exec` 跳板基础设施）
- [x] 1.7 扩展 `UClass`：属性/函数链表、`FindPropertyByName`、`ForEachProperty`、`GetSuperClass`
- [x] 1.8 实现 `FReflectionRegistry` 全局注册与 `FindClassByName`
- [x] 1.9 实现 `FProperty::GetValue`/`SetValue` 类型分发与错误报告
- [x] 1.10 添加 `reflection-runtime` 单元测试：注册、`FindProperty`、读写、`IsA` 兼容

## 2. ReflectionGenerator 工具（方案 B）

- [x] 2.1 纳入 vendored libclang 至 `OpenSpecTest/thirdparty/libclang/`（`include/`、`lib/{Debug,Release}/`、`bin/{Debug,Release}/`），添加 `VERSION` 记录 LLVM 版本
- [x] 2.2 创建 `Tools/ReflectionGenerator/` 工程与 CLI 参数解析（`--compile-commands`、`--scan-dir`、`--output-dir`）
- [x] 2.3 链接 vendored libclang：加载 `compile_commands.json`、创建 TranslationUnit、遍历 `ClassDecl`/`FieldDecl`/`CXXMethod`/`EnumDecl`
- [x] 2.4 从 libclang 提取字段/方法名、类型字符串、父类链、`clang_Type_getOffsetOf`
- [x] 2.5 实现 `MacroLexer`：源文件字符流、标识符、字符串、括号、逗号 token
- [x] 2.6 实现 `MacroParser`：解析 `UPROPERTY(...)`/`UFUNCTION(...)`/`UCLASS(...)` 参数列表与 `meta=(Key=Value)` 子集
- [x] 2.7 实现 `MacroAssociator`：按行号将宏绑定到后续字段/方法/类声明（跳过空行与 `//` 注释）
- [x] 2.8 实现 `CodeEmitter`：输出 `ClassName.generated.h`（`DECLARE_CLASS`、`DECLARE_FUNCTION`）与 `ClassName.gen.cpp`（属性表、`ConstructUClass`、静态注册）
- [ ] 2.9 添加生成器测试：用固定样例头文件验证宏关联、标志解析、offset 输出
- [x] 2.10 在 `thirdparty/libclang/README.md` 记录 LLVM 版本、目录布局、升级替换步骤（无需本机 LLVM 安装）

## 3. CMake 构建集成

- [x] 3.1 顶层/目标 CMake 开启 `CMAKE_EXPORT_COMPILE_COMMANDS`
- [x] 3.2 添加 `ReflectionGenerator` 可执行目标：链接 vendored `libclang.lib`，`POST_BUILD` 复制 `libclang.dll` 至生成器输出目录
- [x] 3.3 确认 `OpenSpecTest` 主目标不链接 libclang（沿用 thirdparty 循环中的 `libclang` skip 逻辑）
- [x] 3.4 为 `OpenSpecTest` 添加 `add_custom_command` 预编译生成步骤（MSVC 无 `compile_commands.json` 时回退到已入库生成物）
- [x] 3.5 将 `OpenSpecTest/Generated/Reflection/` 加入 include 路径并编译 `*.gen.cpp`
- [x] 3.6 首期将生成物纳入版本控制（或提供 fallback 手工注册说明）

## 4. 试点类迁移

- [x] 4.1 为 `FVector` 添加 `USTRUCT` 标记并生成反射描述（`UScriptStruct::GetFVector3Struct` / `GetFRotator3Struct`）
- [x] 4.2 迁移 `USceneComponent`：Transform 字段加 `UPROPERTY`，接入 `GENERATED_BODY` 与生成代码
- [ ] 4.3 迁移 `UMeshComponent` 反射属性（网格相关 `SaveGame` 字段）
- [ ] 4.4 迁移 `UStaticMeshComponent`/`USkeletalMeshComponent` 关键 `SaveGame` 属性
- [ ] 4.5 移除试点类中已被生成代码替代的重复 `StaticClass` 手工注册（保留工厂函数）
- [x] 4.6 验证 `StaticClass()`/`IsA()`/Spawn 路径与迁移前行为一致

## 5. 反射驱动 Details 面板

- [x] 5.1 实现 `FPropertyDetailsBuilder`：遍历属性、按 `Category` 分组、创建 Qt 控件
- [x] 5.2 实现类型控件映射：`bool`/`int`/`float`/`double`/`FVector`/枚举/`string`（首期：bool/float/struct）
- [x] 5.3 改造 `DetailPanelWidget`：选中对象后调用 `FPropertyDetailsBuilder`，移除 Transform 硬编码控件
- [x] 5.4 实现 `DisplayName` meta 与搜索过滤
- [ ] 5.5 手工验收：选中 `USceneComponent` Actor，编辑 Location/Rotation/Scale 后视口同步

## 6. 反射驱动序列化

- [x] 6.1 实现 `FPropertySerializer`：`SaveGame` 属性 JSON 读写（基础类型 + `FVector` + 枚举 + string）
- [ ] 6.2 改造 `ComponentSerializer`：字段读写委托 `FPropertySerializer`，保留类名 `NewObject` 分支
- [ ] 6.3 确保 JSON 键名与现有 `FComponentSaveData` 存档兼容
- [ ] 6.4 添加/更新 `LevelSaveLoadTests` 与组件序列化往返测试
- [ ] 6.5 删除 `SerializeSceneComponent`/`SerializeMeshComponent` 中已被反射替代的手写字段逻辑

## 7. 验证与收尾

- [x] 7.1 全量编译通过（含生成步骤）
- [x] 7.2 运行 `reflection-runtime` 与序列化相关单元测试
- [ ] 7.3 回归：关卡加载、Actor 选中、Transform 编辑、组件存档往返
- [x] 7.4 在 `design.md` 记录已知限制（模板 UCLASS、UObject* 属性、Replicated 未实现）
