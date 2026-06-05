## 新增需求

### 需求:编辑器必须提供拾取性能设置入口
系统必须在主窗口菜单中提供编辑器性能配置入口；系统必须将三角 BVH 分割方式持久化到 INI 文件并在启动时加载。

#### 场景:通过菜单打开性能对话框
- **当** 用户选择「编辑 → 性能」
- **那么** 系统必须显示性能设置对话框，并展示当前 `TriangleBvhSplitMethod` 配置

#### 场景:保存性能设置到 INI
- **当** 用户在性能对话框中确认有效设置
- **那么** 系统必须将设置写入 `Config/EditorPerformance.ini`（或项目约定的等价路径），并在下次启动时恢复

#### 场景:切换分割方式时失效拾取 BVH 缓存
- **当** `TriangleBvhSplitMethod` 由 Median 改为 SAH 或反向变更
- **那么** 系统必须调用 `FEditorPicking::InvalidateTriangleBvhCache()`，禁止复用旧分割方式构建的 BVH

### 需求:三角 BVH 分割方式必须可配置
系统必须支持至少两种三角 BVH 构建策略：Median（默认）与 SAH；系统必须在拾取路径中将该设置传入 `FEditorPicking::PickActor`。

#### 场景:默认使用 Median
- **当** 未提供或首次加载性能配置
- **那么** `TriangleBvhSplitMethod` 必须为 Median

#### 场景:SAH 可用于大网格拾取
- **当** 用户将分割方式设为 SAH 并执行视口拾取
- **那么** 系统必须使用 SAH 策略构建（或重建）对应网格的三角 BVH 并用于 narrow phase 查询

## 修改需求

无。

## 移除需求

无。
