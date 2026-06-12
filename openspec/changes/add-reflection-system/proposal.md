# UE 风格反射系统（libclang + 宏参数解析）

## 为什么

引擎已有 `UObject` / `UClass` 类型层次与 `IsA()` / `StaticClass()` 工厂，但 **没有成员级反射**：属性、方法、枚举元数据均无法在运行时查询。编辑器 `DetailPanelWidget` 手写 Transform 控件，`ComponentSerializer` 按类名分支序列化，每新增一个可编辑字段都要改 UI 和存档代码。这与 UE5 通过 `UPROPERTY` / `UFUNCTION` 驱动 Details 面板、序列化、类型创建的工作流差距巨大，也阻碍了后续脚本、GC、网络复制等基础设施。

现在类型体系已稳定，是实现反射系统的合适时机。技术路线采用 **方案 B**：libclang 解析 C++ 结构 + 独立宏参数 Parser 解析 `UPROPERTY`/`UFUNCTION` 标志位，兼顾 AST 准确性与 UE 风格宏语义。

## 变更内容

- 新增 UE 风格反射宏：`UCLASS`、`USTRUCT`、`UENUM`、`UPROPERTY`、`UFUNCTION`、`GENERATED_BODY`
- 新增构建期工具 `ReflectionGenerator`（libclang + 宏参数 Lexer/Parser），生成 `.generated.h` / `.gen.cpp`
- 扩展运行时 `UClass`，新增 `FProperty`、`UFunction`、`UEnum`、`FStructProperty` 等元数据类型与全局 `FReflectionRegistry`
- 实现属性读写 API（按 offset + 类型分发）与 `ForEachProperty` 遍历
- 将 `DetailPanelWidget` 改为反射驱动：遍历 `EditAnywhere` 属性自动生成控件
- 将 `ComponentSerializer` 中 Scene/Mesh 组件字段迁移为 `SaveGame` 标志驱动的通用属性序列化
- 试点类：`USceneComponent`、`UMeshComponent`、`UStaticMeshComponent`（Transform + Mesh 引用等）
- **不在本期**：蓝图 VM、网络复制、完整 GC 扫描、全量资产类反射、模板 `UCLASS`

## 功能 (Capabilities)

### 新增功能

- `reflection-runtime`：运行时反射元数据（扩展 `UClass`、`FProperty`、`UFunction`、`UEnum`、注册表、属性读写）
- `reflection-codegen`：构建期代码生成器（vendored libclang AST 扫描 + `UPROPERTY`/`UFUNCTION` 宏参数解析 + `.generated.h`/`.gen.cpp` 输出 + CMake 集成）
- `reflection-property-editor`：编辑器 Details 面板基于反射自动生成可编辑属性 UI
- `reflection-property-serialization`：基于 `SaveGame` 标志的通用属性序列化，替代组件手写序列化分支

### 修改功能

无。现有主规范（`world-level-actor-lifecycle` 等）的行为契约不变；组件存档格式可增量扩展（新增反射字段键），不破坏已有字段语义。

## 影响

- **新增**：`OpenSpecTest/src/reflection/` 运行时模块、`Tools/ReflectionGenerator/` 离线工具、`OpenSpecTest/thirdparty/libclang/` vendored 预编译包、反射宏头文件、`*.generated.h` / `*.gen.cpp` 生成物目录
- **修改**：`UClass`/`UObject` 核心、`DetailPanelWidget`、`ComponentSerializer`、`CMakeLists.txt`（生成步骤、`compile_commands.json` 导出、`ReflectionGenerator` 链接 vendored libclang）
- **依赖**：vendored libclang（仅 `ReflectionGenerator` 构建期使用，不链接主程序）、现有 `ObjectRegistry`、`nlohmann::json`
- **不变**：DX12 渲染、资产 `.uasset` 格式、主视口交互、Content Browser
