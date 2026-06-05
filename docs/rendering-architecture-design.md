# 基础渲染体系架构设计

本文档描述从 UObject 继承、参照 UE 架构的基础渲染体系：Component 继承链、Asset 继承链、数据/逻辑/实例分离原则、资源标识与引用、以及可扩展的线程模型。**不涉及具体代码实现**，仅梳理框架与流程。

---

## 一、整体框架与设计原则

### 1.1 三层分离（Data / Logic / Instance）

| 层次 | 代表类型 | 职责 | 生命周期与归属 |
|------|----------|------|----------------|
| **Asset（数据）** | UStaticMesh、USkeletalMesh、材质/纹理等 | 仅存储可复用的渲染数据（顶点、索引、骨骼、材质参数等），不包含游戏状态 | 由资源系统管理，通过「资产相对 data 路径 + Name」唯一标识；Outer/Inner 记录引用树 |
| **Component（实例逻辑）** | UActorComponent → USceneComponent → … | 承载功能：空间变换、可见性、LOD、渲染代理创建与更新、与渲染线程的桥接 | 挂载在 AActor 下，随 Actor 创建/销毁；持有对 Asset 的引用，将 Asset 数据与实例状态结合 |
| **Actor（游戏逻辑）** | AActor | 仅负责游戏逻辑：生成/销毁、Tick、业务规则；不直接持有网格/材质数据 | 由 Level/World 管理，通过 ObjectRegistry 注册 |

原则简述：

- **Actor**：不关心「画什么、怎么画」，只关心「要不要存在、怎么行为」。
- **Component**：关心「用什么 Asset、在什么位置、是否可见、如何提交给渲染」。
- **Asset**：只存数据，可被多个 Component 共享；不依赖任何 Actor/Level。

当前架构不要求高并发渲染，但**预留**后续引入 GameThread / RenderThread / RHIThread / GPU 的扩展点（如 SceneProxy 模式）。

---

## 二、Component 继承体系

### 2.1 继承关系总览

```
UObject
  └── UActorComponent
        └── USceneComponent
              └── UPrimitiveComponent
                    └── UMeshComponent
                          ├── UStaticMeshComponent
                          └── USkinnedMeshComponent
                                └── USkeletalMeshComponent
```

### 2.2 各组件设计功能

#### UActorComponent（基类：功能单位）

- **定位**：所有「挂到 Actor 上的功能块」的基类。
- **职责**：
  - 与 Owner（AActor）关联：归属关系、生命周期随 Actor。
  - 提供可重写生命周期：注册/注销、初始化、Tick（若需要）。
  - 不包含空间信息，不参与场景层级。
- **设计要点**：纯逻辑/功能抽象，为 SceneComponent 与后续渲染相关 Component 提供「可挂载」的契约。

#### USceneComponent（带空间信息的组件）

- **定位**：具有「在场景中的位置与朝向」的组件基类。
- **职责**：
  - **相对变换**：相对父 Component 的平移、旋转、缩放（可选用 FTransform 或位置+旋转+缩放）。
  - **层级**：父子关系（Attach/Detach），形成场景树；根一般为 Actor 的 RootComponent。
  - **世界变换**：由相对变换与父链计算世界变换，供渲染与物理使用。
- **设计要点**：所有需要在场景里「占位」的组件都从此派生；不涉及「是否可渲染」，仅空间。

#### UPrimitiveComponent（可渲染/可碰撞的基元）

- **定位**：代表「在场景中有几何意义」的物体，可被渲染和/或参与碰撞。
- **职责**：
  - **渲染可见性**：是否参与绘制（如 SetVisibility）、遮挡等策略的接口预留。
  - **Bounds**：包围体（AABB/球体），用于视锥剔除、遮挡、LOD 等（可先简单实现，后续扩展）。
  - **与渲染层桥接**：为创建/更新「渲染代理」（后续的 SceneProxy 对应物）预留接口，当前可为空或简单实现。
- **设计要点**：仍不持有具体网格数据，只表达「这是一块可画的几何体」；具体几何由 UMeshComponent 及其子类与 Asset 绑定。

#### UMeshComponent（绑定网格资源的组件）

- **定位**：将「网格 Asset」与「场景中的一份实例」绑定。
- **职责**：
  - **Mesh 引用**：持有对 UStaticMesh 或 USkeletalMesh（或更抽象：UMeshAsset）的引用；通过资产标识（data 相对路径 + Name）解析。
  - **材质覆盖**：可选：每槽位覆盖材质、颜色等（与 Asset 默认材质分离）。
  - **LOD**：LOD 选择策略的接口（当前可简化为一档），为后续多 LOD 留扩展。
- **设计要点**：从 Asset 取几何数据，结合 Component 的变换与覆盖参数，得到「本实例的绘制描述」；不直接存顶点/索引，只存引用与覆盖状态。

#### UStaticMeshComponent（静态网格实例）

- **定位**：渲染「不可变形」的静态网格。
- **职责**：
  - 引用 UStaticMesh Asset。
  - 将 Asset 的顶点/索引 + 本 Component 的世界变换 + 材质/覆盖，提交给渲染管线（当前可为单线程，后续可交给 RenderThread 的 SceneProxy）。
- **设计要点**：无骨骼、无蒙皮，每帧仅需世界矩阵等少量数据，适合大量静态物体。

#### USkinnedMeshComponent（蒙皮网格基类）

- **定位**：支持「由骨骼驱动顶点」的网格的基类。
- **职责**：
  - **骨骼与蒙皮**：与 USkinnedAsset（如 USkeletalMesh）对应；持有骨骼层级、蒙皮权重等概念的接口。
  - **姿态/动画**：为「当前帧骨骼变换」提供接口（数据可来自动画系统或外部驱动）；不要求本阶段实现完整动画，但接口预留。
- **设计要点**：区分「资源侧」（USkinnedAsset 存骨骼拓扑与蒙皮数据）与「实例侧」（本 Component 存当前姿态、可见性等）。

#### USkeletalMeshComponent（骨骼网格实例）

- **定位**：渲染骨骼蒙皮网格的具象实现。
- **职责**：
  - 引用 USkeletalMesh Asset。
  - 每帧（或按需）根据骨骼姿态计算蒙皮后的顶点（或将骨骼矩阵交给 GPU 蒙皮），再提交绘制。
- **设计要点**：与 UStaticMeshComponent 并列，一条走静态几何，一条走动态蒙皮几何；二者都只负责「实例逻辑」，数据均在 Asset。

---

## 三、Asset 继承体系

### 3.1 继承关系总览

```
UObject
  └── UStreamableRenderAsset
        ├── UStaticMesh
        └── USkinnedAsset
              └── USkeletalMesh
```

### 3.2 各资产设计功能

#### UStreamableRenderAsset（可渲染资源的基类）

- **定位**：所有「可被渲染管线使用」的资源的基类。
- **职责**：
  - **资源标识**：与全局资源管理约定一致，使用「资产相对 data 路径 + Name」作为唯一标识符；便于引用、保存、加载。
  - **流式加载扩展点**：当前不实现流式加载，但预留接口（如加载状态、分块 LOD 数据等），便于后续接入按需加载、流式纹理/网格。
- **设计要点**：仅做抽象与标识约定，不承载具体几何类型。

#### UStaticMesh（静态网格数据）

- **定位**：不可变形网格的纯数据容器。
- **职责**：
  - 顶点缓冲、索引缓冲（及可选顶点布局描述）。
  - 子网格/材质槽位划分（Section/MaterialIndex），与 UMeshComponent 的材质覆盖对应。
  - 可选：LOD 链（多级几何），当前可仅支持 LOD0。
- **设计要点**：无骨骼、无动画；导入后数据不变，仅被引用。

#### USkinnedAsset（蒙皮资源基类）

- **定位**：带骨骼与蒙皮信息的资源的基类。
- **职责**：
  - 骨骼层级（Skeleton 或等效结构）、参考姿态。
  - 蒙皮数据：顶点-骨骼索引与权重。
  - 与 USkinnedMeshComponent 对应，提供「数据侧」的蒙皮描述。
- **设计要点**：可被动画系统引用（骨骼名、槽位等），为后续动画蓝图/状态机留扩展。

#### USkeletalMesh（骨骼网格数据）

- **定位**：骨骼蒙皮网格的完整数据资产。
- **职责**：
  - 继承 USkinnedAsset 的骨骼与蒙皮数据。
  - 每个 LOD 的顶点/索引及与骨骼的绑定；多 LOD 可后续扩展。
- **设计要点**：与 UStaticMesh 并列，一个静态一个蒙皮；均只存数据，由 Component 实例化。

---

## 四、资源标识与引用树（Outer / Inner）

### 4.1 唯一标识符

- **约定**：以「**资产相对 data 的路径 + Name**」作为资产的唯一标识符（例如：`meshes/character/soldier` + Name `"Soldier"`）。
- **用途**：序列化引用、资源表键、加载时查找；与现有 ObjectRegistry 的 ObjectId 可并存（ObjectId 用于运行时对象，资产路径+Name 用于资源身份）。

### 4.2 Outer / Inner 与引用树

- **Outer**：对象在「逻辑归属」上的父对象（如某材质以某材质库为 Outer，某 LOD 以某 Mesh 为 Outer）。用于：
  - 形成资源树，便于批量加载/卸载、依赖分析。
  - 保存/加载时递归序列化或按树引用。
- **Inner**：某对象下「直接拥有的子对象」集合；Outer 的反向关系。
- **引用树**：Asset 之间通过 Outer/Inner 形成树；Component 引用 Asset 时使用「资产路径+Name」或 ObjectId，不改变 Asset 树的归属。这样既满足「用路径+Name 做唯一标识」，又满足「用 Outer/Inner 记录引用树」的要求。

### 4.3 与 Assimp 导入的衔接

- **导入入口**：由资源管线（或专门 Importer）使用 Assimp 解析外部模型文件（如 fbx/obj/gltf）。
- **产出**：
  - 生成 UStaticMesh 或 USkeletalMesh（及可选 UMaterial 等），写入项目 data 目录或中间资产格式。
  - 为每个生成的 Asset 设置好「资产相对 data 路径 + Name」，并建立 Outer/Inner（例如：Mesh 为 Outer，多个 LOD 或 Section 为 Inner；或材质库为 Outer，材质为 Inner）。
- **引用记录**：导入结果中的引用（Mesh→材质、SkeletalMesh→Skeleton 等）用上述标识符或 Inner 链记录，便于后续加载时解析依赖。

---

## 五、整体流程概览

### 5.1 资源侧（Asset）

1. **导入**：Assimp 解析外部模型 → 生成 UStaticMesh / USkeletalMesh（及关联资源）→ 以「data 相对路径 + Name」命名，并建立 Outer/Inner 引用树 → 持久化到项目 data。
2. **加载**：根据「路径+Name」查找或加载 Asset，反序列化到 UObject 树；需要时通过引用树加载依赖（如材质、骨骼）。
3. **运行时**：Asset 仅作为只读数据被 Component 引用，不参与 Tick。

### 5.2 实例侧（Component + Actor）

1. **创建**：Level/游戏逻辑 Spawn Actor → 创建 AActor 及根 USceneComponent（或挂载 UStaticMeshComponent / USkeletalMeshComponent）→ Component 通过「路径+Name」或句柄引用对应 UStaticMesh/USkeletalMesh。
2. **每帧**：
   - **GameThread**：Actor Tick → Component Tick；SceneComponent 更新世界变换；MeshComponent 根据可见性、LOD 等决定是否提交绘制；当前实现可直接在这一步生成 DrawCall 或渲染命令。
   - **后续扩展**：MeshComponent 创建/更新「SceneProxy」或等效结构，由 RenderThread 消费并构建 DrawCall；RHIThread 提交 GPU；GPU 执行绘制。

### 5.3 渲染侧（当前与后续）

- **当前**：可不区分线程，在 GameThread 或主线程内完成：收集可见的 PrimitiveComponent → 取 Mesh Asset 数据 + Component 变换与材质 → 调用现有 Dx12Renderer 的绘制接口。
- **后续**：引入 RenderThread 后，Component 只提交「渲染描述」（如 SceneProxy），RenderThread 据此构建 DrawCall；RHIThread 负责 GPU API；保持「Asset 只存数据、Component 只做实例逻辑、Actor 只做游戏逻辑」的边界不变。

---

## 六、线程模型预留（不要求当前实现）

| 线程 | 职责 |
|------|------|
| **GameThread** | 游戏逻辑、Actor、Component 的 Tick 与状态更新；创建/更新 SceneProxy 或渲染描述并投递到 RenderThread |
| **RenderThread** | 根据 SceneProxy 构建 DrawCall、管理渲染状态、提交到 RHI |
| **RHIThread** | 执行 GPU API（CommandList 录制、提交），与具体图形 API 解耦 |
| **GPU** | 实际执行绘制 |

- **SceneProxy**：每个 UPrimitiveComponent（或 UMeshComponent）在渲染侧可有对应 Proxy 对象，仅存渲染所需数据（世界矩阵、材质句柄、顶点/索引句柄等）；GameThread 更新 Component 后，将差异同步到 Proxy，RenderThread 只读 Proxy 构建 DrawCall。当前架构可先不实现 Proxy，仅保留「Component 提供可被渲染层消费的接口」这一扩展点。

---

## 七、与现有工程的衔接点

- **UObject**：已有 ObjectId、ObjectName、UClass、Flags、ReferencedObjectIds；需扩展 **Outer/Inner** 用于 Asset 引用树；资产标识「路径+Name」可与 ObjectRegistry 并存（资源表用路径，运行时对象用 ObjectId）。
- **AActor**：已有 Transform、Tick、PendingDestroy；需引入 **RootComponent**（USceneComponent*）及可选的 Component 列表，Actor 只做逻辑，空间与渲染交给 Component。
- **ULevel / World**：SpawnActor 时除创建 Actor 外，可创建并挂载 Component；Destroy 时随 Actor 一并销毁 Component。
- **Dx12Renderer**：保持为「接收绘制命令并执行」的底层；上层由「渲染收集器」或后续的 RenderThread 根据 Component/SceneProxy 生成绘制调用；Mesh 数据来源于 UStaticMesh/USkeletalMesh 的顶点/索引缓冲（需与 D3D12 缓冲格式对接）。
- **ResourceLoader / 资源管线**：现有 LoadModelAsset 可演进为「返回 UStaticMesh/USkeletalMesh 或资产句柄」的加载器；Assimp 导入结果写入 data 并登记「路径+Name」与 Outer/Inner。

---

## 八、小结

- **Component 链**：UActorComponent → USceneComponent → UPrimitiveComponent → UMeshComponent → UStaticMeshComponent / USkinnedMeshComponent → USkeletalMeshComponent；职责清晰：空间、可见性、网格引用、静态/蒙皮分支。
- **Asset 链**：UStreamableRenderAsset → UStaticMesh / USkinnedAsset → USkeletalMesh；只存数据，流式与多 LOD 预留扩展。
- **原则**：Asset 只存数据，Component 负责实例逻辑与渲染桥接，Actor 只负责游戏逻辑；资源用「data 相对路径+Name」标识，用 Outer/Inner 记录引用树；Assimp 导入产出上述 Asset 并写入引用关系；线程模型与 SceneProxy 预留，便于后续高并发渲染扩展。

以上为整体框架与各组件设计功能说明，不生成代码，便于后续按模块实现与迭代。
