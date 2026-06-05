## 新增需求

### 需求:视口左键必须能拾取活动关卡中的 Actor
系统必须在渲染视口内将左键点击的屏幕坐标转换为世界空间拾取射线，并在当前活动关卡已加载的 Actor 集合中执行命中检测；系统必须通过 `GameApp::SelectActor` 更新选中状态，禁止在 UI 层维护独立的选中 ID 副本。

#### 场景:点击命中 Actor 时选中最近者
- **当** 用户在视口聚焦状态下左键点击，且拾取射线与至少一个可拾取 Actor 的包围体相交
- **那么** 系统必须选中沿拾取射线距相机最近（射线参数 \(t\) 最小且 \(t \ge 0\)）的 Actor，并触发场景修订以使大纲与 Detail 面板刷新

#### 场景:点击空白时清除选中
- **当** 用户左键点击且拾取射线未与任何可拾取 Actor 相交
- **那么** 系统必须调用 `SelectActor(0)` 清除当前选中

#### 场景:拾取射线必须与渲染相机一致
- **当** 系统构建拾取射线
- **那么** 系统必须使用与 `Dx12Renderer` 场景渲染相同的视图矩阵、投影矩阵、视口宽高及近远裁剪面参数，禁止在拾取路径中使用独立硬编码相机公式

### 需求:拾取必须排除不可见与被裁剪的 Actor
系统必须仅对可参与拾取的 Actor 进行命中排序；系统禁止选中视锥外或被距离裁剪排除的 Actor。

#### 场景:不可见 Primitive 不参与拾取
- **当** Actor 的 `UPrimitiveComponent` 满足 `IsVisible() == false`
- **那么** 该 Primitive 不得参与该 Actor 的拾取包围体合并

#### 场景:视锥外 Actor 不可选中
- **当** Actor 合并后的世界空间 AABB 完全位于当前相机视锥之外
- **那么** 系统必须将该 Actor 排除在拾取候选之外

#### 场景:超出 CullDistance 的 Actor 不可选中
- **当** Primitive 的 `CullDistance > 0` 且相机位置到包围体原点的距离大于 `CullDistance`
- **那么** 系统必须将该 Primitive 排除；若 Actor 所有 Primitive 均被排除且无任何有效包围体，则该 Actor 不可被拾取选中

### 需求:无网格 Actor 必须具备可拾取包围体
系统必须为缺少可见网格资产的 Actor 提供保守默认包围体，避免合法 Actor 无法被点选。

#### 场景:无 Primitive 时使用默认球体包围
- **当** Actor 不存在可见 Primitive 或合并包围体无效
- **那么** 系统必须使用以 `ActorTransform.Position` 为中心、固定最小半径的默认世界空间包围体参与射线检测

### 需求:拾取不得干扰 Gizmo 拖拽起始
系统必须在左键按下时优先判定 Transform Gizmo 命中；仅当未命中 Gizmo 时才执行 Actor 拾取。

#### 场景:Gizmo 命中时不切换选中
- **当** 左键按下位置命中当前选中 Actor 的 Transform Gizmo 操控器
- **那么** 系统必须进入 Gizmo 拖拽流程，且不得因拾取逻辑更换当前选中 Actor

### 需求:可见网格必须使用三角面 Narrow Phase 拾取
系统必须对 `UStaticMeshComponent` 与 `USkeletalMeshComponent` 在具备有效三角索引时执行三角面级命中检测；系统禁止使用仅 OBB/屏幕 fallback 作为最终命中结果（无三角几何时可回退到 OBB 或默认包围体）。

#### 场景:StaticMesh 使用 Möller–Trumbore 求交
- **当** 拾取射线通过 StaticMesh 的 broad phase 且网格存在有效 `Vertices` 与 `Indices`
- **那么** 系统必须使用双面 Möller–Trumbore 算法在局部空间射线与三角面求交，并将最近命中转换为世界空间射线参数 \(t\)

#### 场景:SkeletalMesh 使用蒙皮顶点求交
- **当** 拾取射线通过 SkeletalMesh 的 broad phase 且存在有效 `SkinVertices` 与 `Indices`
- **那么** 系统必须使用与 StaticMesh 相同的三角 BVH narrow phase 流程（顶点取自 `GetSkinVertices().Position`）

#### 场景:每个网格一棵可缓存的三角 BVH
- **当** 某网格首次参与 narrow phase 或缓存因设置/数据变更失效
- **那么** 系统必须按「网格指针 + 分割方式 + 顶点/索引数量」构建并缓存 `FEditorTriangleBvh`；后续拾取必须复用缓存，禁止每帧重复建树

### 需求:拾取必须使用 Actor 级 BVH 与单次网格查询
系统必须使用关卡 Actor 世界 AABB BVH 作为 broad phase 候选过滤；系统对每个候选 Actor 的每个网格必须合并为**一次**三角 BVH 查询（全索引范围），禁止对每个 Section 重复全树遍历。

#### 场景:存在 BVH 候选时仅测试候选 Actor
- **当** Actor 级 BVH 查询返回非空候选列表
- **那么** 系统必须仅对候选 Actor 执行 detailed pick，禁止合并测试关卡内全部 Actor

#### 场景:Narrow phase 使用当前最佳命中收紧 MaxT
- **当** 对某网格执行三角 BVH `Query`
- **那么** 系统必须将世界空间 `BestT` 转换为局部空间 `LocalMaxT` 传入查询，以提前剪枝远侧节点

### 需求:拾取不得因连续点击导致可感知卡顿
系统必须保证在 BVH 已缓存的前提下，同点连续拾取不得触发每帧全量 BVH 重建；系统必须将单次 pick 的 narrow phase 查询次数控制在每 mesh 每 pick 一次（大网格多 Section 亦同）。

#### 场景:缓存命中时无建树开销
- **当** 用户对同一 StaticMesh 连续左键拾取且网格数据与分割方式未变
- **那么** 系统必须命中三角 BVH 缓存，且不得出现与首次点击相当的建树耗时

## 修改需求

### 需求:拾取必须排除不可见与被裁剪的 Actor
（原 broad phase 为 Ray-AABB；现 narrow phase 在 Primitive OBB/Section 过滤后使用三角 BVH，最近 \(t\) 仍以世界射线为准。）

## 移除需求

### 需求:拾取仅使用合并世界 AABB 的 Slab 求交作为最终命中
**原因**：已由三角 BVH narrow phase 取代；AABB/Obb 仅作 broad phase。
**迁移**：`EditorPicking` 保留 Actor BVH 与 Section/Obb 过滤，最终命中必须来自 `FEditorTriangleBvh::Query` 或明确的无几何回退路径。
