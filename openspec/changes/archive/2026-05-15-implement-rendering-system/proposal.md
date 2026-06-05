# 渲染组件与资产体系实现提案

## 上下文

当前工程已具备 `World -> Level -> Actor` 运行时结构、`UObject` 对象系统、以及 DX12 渲染管线。场景中静态地图模型已通过 `Level` 的 `mapModelPath` 与渲染器对接，但尚缺少完整的渲染组件与资产体系：

- **Component 侧**：仅有 `AActor` 的 `Transform`，未抽象出场景层级（SceneComponent）、可渲染基元（PrimitiveComponent）、网格实例（MeshComponent）等渲染功能单元。
- **Asset 侧**：渲染数据（顶点/索引/材质）直接由 `Dx12Renderer` 内部管理，未抽象为可复用的 `UStaticMesh`、`USkeletalMesh` 等资产对象。

本次变更是对「基础渲染体系设计文档」的实现落地，采用 Data / Logic / Instance 分离原则：Asset 只存数据、Component 负责功能与渲染桥接、Actor 只负责游戏逻辑。

## 目标 / 非目标

**目标：**

- 实现 Component 继承链：`UActorComponent` → `USceneComponent` → `UPrimitiveComponent` → `UMeshComponent` → `UStaticMeshComponent` / `USkinnedMeshComponent` → `USkeletalMeshComponent`。
- 实现 Asset 继承链：`UStreamableRenderAsset` → `UStaticMesh` / `USkinnedAsset` → `USkeletalMesh`。
- 将现有 Actor 的 Transform 迁移至 RootComponent（USceneComponent），实现空间层级。
- 为 Component 与 Asset 建立资源引用机制（以「data 相对路径 + Name」为唯一标识）。
- 扩展 `UObject` 的 Outer/Inner 能力，记录资产引用树。
- 预留后续多线程渲染、SceneProxy、流式加载的扩展点。

**非目标：**

- 不在本变更实现完整的 RenderThread / RHIThread 并发渲染。
- 不在本变更实现异步流式加载，仅预留接口。
- 不在本变更实现动画系统（骨骼动画播放），仅预留蒙皮数据与 Component 接口。
- 不在本变更重构现有 DX12 渲染管线核心，仅完成必要的资产绑定与调用适配。

## 主要相关方

- **游戏逻辑迭代**：需要 Actor 通过 Component 挂载网格、设置可见性、控制 LOD。
- **渲染/性能迭代**：需要从 Asset 加载几何数据、支持多实例提交、为后续 SceneProxy 扩展提供入口。
- **资源管线**：需要通过 Assimp 导入生成 `UStaticMesh` / `USkeletalMesh`，建立 Outer/Inner 引用树。
- **数据持久化**：需要保存 Actor-Component 关系与 Asset 引用，支持关卡序列化为 JSON。

## 决策

### 决策 1：Component 体系采用 UE 经典层级

- **选择**：从 `UActorComponent` 开始，逐级递增职责：`USceneComponent`（空间变换）、`UPrimitiveComponent`（可见性、Bounds、渲染代理接口）、`UMeshComponent`（网格引用）、`UStaticMeshComponent` / `USkinnedMeshComponent`（静态/蒙皮实例）。
- **原因**：职责清晰，便于后续扩展（如光照组件、粒子组件），与 UE 生态对齐降低学习成本。
- **备选**：
  - 扁平化设计（所有渲染功能平铺在 Actor）—— 复用度低，后续难以扩展。
  - 直接引入 ECS 渲染组件 —— 当前阶段复杂度偏高。

### 决策 2：Asset 体系与 UObject 共用对象系统

- **选择**：`UStaticMesh`、`USkeletalMesh` 继承自 `UObject`（经由 `UStreamableRenderAsset`），通过 `ObjectRegistry` 管理生命周期，与 Actor/Component 使用同一套 ID 系统。
- **原因**：统一对象追踪与序列化机制，避免两套内存管理逻辑。
- **备选**：
  - Asset 独立于 UObject —— 增加序列化/GC 复杂度。
  - 仅用文件路径引用 Asset —— 缺少运行时对象化能力，无法支持编辑器编辑与热重载。

### 决策 3：资源标识采用「data 相对路径 + Name」

- **选择**：以「资产相对 data 目录的路径 + Name」作为唯一标识（例如 `meshes/character/soldier`），与现有的 `ObjectId` 并存（ObjectId 用于运行时对象，路径+Name 用于资源定位）。
- **原因**：资源通常以文件形式存在，便于编辑器定位与人工管理；运行时通过 ResourceLoader 解析为 UObject。
- **备选**：
  - 仅用 ObjectId —— 丢失资源文件可读性，调试困难。
  - 仅用文件路径 —— 缺少 UObject 统一生命周期管理。

### 决策 4：Outer/Inner 仅用于引用树记录

- **选择**：为 `UObject` 添加 Outer/Inner 字段，用于记录资产间的拥有关系（如材质库 → 材质、Mesh → LOD），不参与运行时属性查找。
- **原因**：保持序列化与依赖分析的完整性；运行时查找通过资源表（ResourceRegistry）完成。
- **备选**：
  - 用 Outer/Inner 做运行时属性查找 —— 增加查询开销且语义混杂。

### 决策 5：首版采用主线程渲染提交

- **选择**：Component 在 GameThread 完成渲染描述生成，直接调用 `Dx12Renderer` 绘制，暂不引入 RenderThread。
- **原因**：MVP 复杂度最小化，优先验证架构正确性；接口层预留 SceneProxy 扩展点，后续可平滑迁移到 RenderThread。
- **备选**：
  - 首版即多线程 —— 引入同步复杂度，延迟验证周期。

## 风险 / 权衡

- [Component 层级过深增加调用成本] → 通过内联/编译器优化缓解；首版以功能正确性优先，后续 PROFILE 驱动优化。
- [Asset 序列化体积膨胀] → 当前仅存储顶点/索引原始数据，后续可引入压缩或流式分块。
- [Outer/Inner 与 ObjectId 混用造成混淆] → 在代码与文档中明确区分：ObjectId = 运行时对象身份，路径+Name = 资源定位，Outer/Inner = 逻辑拥有关系。
- [首版主线程渲染在复杂场景可能出现卡顿] → 作为已知权衡接受；接口层已预留 SceneProxy 迁移路径。

## 回滚策略

- 保留现有 `AActor::Transform` 作为降级路径；若 Component 体系出现严重稳定性问题，可回退到 Actor 直接持有变换数据。
- 资源加载失败时fallback 到默认网格，避免渲染管线崩溃。

---

**提案状态**：草稿  
**创建日期**：2026-03-12  
**关联设计文档**：`docs/rendering-architecture-design.md`
