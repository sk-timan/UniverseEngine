## 为什么

当前项目已具备 DX12 渲染循环与基础配置编辑能力，但尚未建立 `World/Level/Actor` 运行时结构，导致关卡加载/释放、场景元素生命周期与后续玩法系统（鱼点、可钓区、交互物）无法在统一模型下扩展。现在引入该结构，可以把“地图管理”与“渲染细节”解耦，降低后续 Phase 2-5 迭代耦合风险。

## 变更内容

- 新增 `World` 层，负责关卡加载、卸载、切换与世界级 Tick 入口。
- 新增 `Level` 层，负责关卡内实体集合管理、生命周期状态（Loading/Loaded/Unloading）与资源占用边界。
- 新增 `Actor` 基类，统一关卡元素的标识、变换、更新与销毁语义。
- 将 `gameplay` 配置中的 `world.map_id` 接入到关卡装载流程，形成“配置 -> 关卡实例”闭环。
- 为后续功能预留扩展点：可钓区域、鱼类刷点、交互物与调试对象均通过 Actor 模型挂载。

## 功能 (Capabilities)

### 新增功能
- `world-level-actor-lifecycle`: 提供世界、关卡与实体的统一生命周期管理能力，支持加载、释放、逐帧更新与关卡切换时的安全回收。

### 修改功能
- 无。

## 影响

- 受影响代码：
  - `OpenSpecTest/src/app`（应用启动与 Tick 流程将接入 World）
  - `OpenSpecTest/src/world`（新增 world/level/actor 相关模块）
  - `OpenSpecTest/src/render`（从直接地图路径驱动过渡为由 World/Level 提供场景数据）
  - `OpenSpecTest/src/data`（`map_id` 与关卡定义映射关系）
- 对现有能力影响：
  - 不改变 `gameplay-config-editor` 既有需求语义，仅扩展其 `map_id` 的消费路径。
- 风险与兼容性：
  - 属于内部架构演进，预期无外部 API BREAKING 变更。
