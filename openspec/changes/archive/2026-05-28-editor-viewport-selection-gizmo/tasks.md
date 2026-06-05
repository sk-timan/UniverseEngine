## 1. 编辑器模块骨架

- [x] 1.1 创建 `OpenSpecTest/src/editor/` 目录并添加 `EditorTypes.h`（`EGizmoMode`、`EPickResult`、`FGizmoDragState` 等）
- [x] 1.2 将 `editor` 源文件加入 `OpenSpecTest/CMakeLists.txt` 并确保可编译链接

## 2. 共享视图矩阵

- [x] 2.1 实现 `EditorViewMatrices`（或等价工具）：从 `CameraState`、视口尺寸、近远裁剪面构建与 `Dx12Renderer::UpdateCameraConstants` 一致的 View/Projection
- [x] 2.2 提供屏幕像素 → 世界拾取射线 API（含 Qt Y 轴翻转）
- [x] 2.3 让 `Dx12Renderer` 复用同一矩阵构建路径，避免拾取与渲染漂移

## 3. 视口拾取（editor-viewport-picking）

- [x] 3.1 实现 `EditorPicking`：遍历活动关卡 Actor，合并可见 `UPrimitiveComponent` 世界 AABB
- [x] 3.2 实现 Ray-AABB（Slab）相交，按最小 \(t \ge 0\) 选取 Actor
- [x] 3.3 实现视锥保守剔除与 `CullDistance` 过滤
- [x] 3.4 为无 mesh Actor 提供默认世界空间包围体
- [x] 3.5 在 `GameApp` 暴露 `PickActorAtViewportPosition` 并委托 `EditorPicking`

## 4. 视口输入与选中联动

- [x] 4.1 在 `RenderViewportWidget` 处理左键按下/移动/释放，转发视口像素坐标到 `GameApp`
- [x] 4.2 左键未命中 Gizmo 时调用拾取并 `SelectActor`；命中空白时 `SelectActor(0)`
- [x] 4.3 选中变化时 `BumpSceneRevision`，确保 `WorldContentPanel` 与 `DetailPanel` 刷新
- [x] 4.4 补齐世界大纲当前选中项高亮（`SyncTreeSelection`）

## 5. Transform Gizmo 核心（editor-transform-gizmo）

- [x] 5.1 实现 `EditorTransformGizmo`：锚点取自 Root `GetWorldLocation()` 或 `ActorTransform.Position`
- [x] 5.2 实现平移/旋转/缩放三种 Gizmo 线段几何生成（屏幕恒定尺度）
- [x] 5.3 在 `Dx12Renderer` 增加 `DrawEditorGizmo`，复用 Line PSO 于场景绘制之后渲染
- [x] 5.4 在 `GameApp` 集成 `GetGizmoMode` / `SetGizmoMode` 与 `TickEditorInteraction`
- [x] 5.5 实现 `W`/`E`/`R` 边沿检测切换模式（非鼠标环视时生效）

## 6. 相机输入解耦（BREAKING）

- [x] 6.1 将 `WASD`+`QE` 相机移动限制为仅在 `m_is_mouse_look_active_` 时生效
- [x] 6.2 非环视时禁止 `W`/`E`/`R` 驱动相机，专用于 Gizmo 模式切换
- [x] 6.3 验证右键环视 + WASD 与 UE 习惯一致

## 7. Gizmo 拖拽与 Transform 写入

- [x] 7.1 实现 Gizmo 轴/环命中检测（像素容差）及拖拽状态机
- [x] 7.2 实现平移轴拖拽求解并写回 Transform
- [x] 7.3 实现旋转环拖拽求解（`FRotator3` 约定）并写回 Transform
- [x] 7.4 实现缩放轴拖拽求解，强制 Scale 最小正值 clamp
- [x] 7.5 左键按下优先 Gizmo 命中，避免拖拽起始时切换选中

## 8. Detail 面板双向同步

- [x] 8.1 扩展 `DetailPanelWidget::RefreshFromSelection`：同一 Actor 在 `SceneRevision` 变化时重新 `PopulateFromActorTransform`
- [x] 8.2 保持 `m_is_syncing_controls_` 防止 Gizmo ↔ Detail 回环
- [x] 8.3 验证 Detail 手改 Transform 后 Gizmo 下一帧对齐

## 9. 三角面 Narrow Phase 与拾取性能（会话补充）

- [x] 9.1 实现 `FEditorTriangleBvh`（Median/SAH）与 `FEditorTriangleBvhCache`
- [x] 9.2 StaticMesh / SkeletalMesh 使用 Möller–Trumbore narrow phase，移除 Screen Fallback
- [x] 9.3 Actor 级 `FEditorPickBvh` 候选过滤；每 mesh 单次 `Query` + `LocalMaxT` 剪枝
- [x] 9.4 修复 StaticMesh 同点连点掉帧（避免 per-section 全树遍历）

## 10. 编辑器性能设置（editor-performance-settings）

- [x] 10.1 `FEditorPerformanceSettings` + `EditorPerformanceStore`（INI 持久化）
- [x] 10.2 `EditorPerformanceDialog` + 主菜单「编辑 → 性能」
- [x] 10.3 `GameApp` 加载/应用设置；分割方式变更时 `InvalidateTriangleBvhCache`

## 11. Gizmo 拖拽性能（会话补充）

- [x] 11.1 `SetSelectedActorTransform` 移除 `RefreshActiveLevelRender`，避免双次 Present
- [x] 11.2 拖拽中直接 `SetActorTransform`；无 Transform delta 不写回
- [x] 11.3 拖拽结束（`OnViewportLeftMouseRelease`）时 `BumpSceneRevision` 一次

## 12. 验证与回归（收尾 — 需人工勾选）

- [x] 12.1 左键选中最近可见 Actor，点空白取消，大纲/Detail 同步
- [x] 12.2 视锥外与超 `CullDistance` Actor 不可选中
- [x] 12.3 大 StaticMesh 同点连点：无明显掉帧（对比修复前）
- [x] 12.4 `W`/`E`/`R` 切换 Gizmo；拖拽三模式改 Transform，松手后 Detail 数值一致
- [x] 12.5 环视时 WASD+QE 移动相机；非环视时 `W`/`E`/`R` 不移动相机
- [x] 12.6 Gizmo 拖拽帧率接近正常 60（VSync 下），非稳定 ~30
- [x] 12.7 快速连点 Gizmo 同点：无明显掉帧
- [x] 12.8 编辑 → 性能：切换 Median/SAH 后拾取仍正确，首次拾取可接受建树延迟
- [x] 12.9 回归：右键环视、地图加载、模型导入、Detail 手改路径无崩溃
