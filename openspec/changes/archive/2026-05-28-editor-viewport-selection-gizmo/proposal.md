## 为什么

编辑器已具备 Actor 选中状态、`Detail` 面板 Transform 编辑与世界大纲点选，但视口内无法通过鼠标点选场景对象，也无法用 UE5 风格的 Transform Gizmo 直观调整位置/旋转/缩放。在摆放关卡物体、调试钓点与场景布局时，仅依赖数值输入效率低且易与场景脱节。现在补齐视口拾取与 Gizmo 交互，可形成「视口 ↔ 大纲 ↔ Detail」一致的编辑闭环，并为后续钓鱼 MVP 场景搭建提供基础工具。

## 变更内容

- 新增视口左键拾取：在未被视锥/距离裁剪的可见 Actor 中，选中沿拾取射线距相机最近的一个；点击空白取消选中。
- 新增 Transform Gizmo：在选中 Actor Root 世界坐标处绘制平移/旋转/缩放操控器；支持拖拽修改 `FActorTransform`。
- 新增 Gizmo 模式快捷键：`W` / `E` / `R` 分别切换 Location / Rotation / Scale 编辑模式（对齐 UE5）。
- 扩展 Detail 与世界大纲同步：Gizmo 拖拽、视口点选、大纲点选三处变更均反映到 `DetailPanelWidget` 的 Transform 控件。
- **BREAKING**：视口聚焦且未按住右键「鼠标环视」时，`W` / `E` / `R` 用于 Gizmo 模式而非相机移动；相机 `WASD` + `QE` 移动仅在右键环视激活时生效（与 UE 视口飞行习惯对齐）。
- **增强（会话补充）**：三角面 Narrow Phase 拾取（Möller–Trumbore + 每网格三角 BVH 缓存）；Actor 级 BVH 候选过滤；每 mesh 单次 BVH 查询与 `LocalMaxT` 剪枝；取消 Screen Fallback。
- **增强（会话补充）**：编辑器性能设置（`编辑 → 性能`）：三角 BVH Median/SAH 可配置并持久化，切换时失效 BVH 缓存。
- **增强（会话补充）**：Gizmo/拾取性能修复——拖拽单路径 Present、无 Transform 变化不写回、拖拽结束再 `BumpSceneRevision`。

## 功能 (Capabilities)

### 新增功能

- `editor-viewport-picking`: 视口拾取管线（Actor BVH broad phase + 三角 BVH narrow phase、视锥/`CullDistance`、最近 Actor），与 `GameApp::SelectActor` 集成。
- `editor-transform-gizmo`: Transform Gizmo 渲染、轴/环命中、拖拽求解、`W/E/R` 模式、Detail 双向同步与单路径渲染性能约束。
- `editor-performance-settings`: 拾取 BVH 分割方式（Median/SAH）的配置 UI、INI 持久化与缓存失效。

### 修改功能

- 无（现有 `world-level-actor-lifecycle`、`gameplay-config-editor` 规范行为不变；本变更为编辑器交互层扩展）。

## 影响

- **新增模块**：`OpenSpecTest/src/editor/`（拾取、Gizmo、编辑器交互类型）。
- **核心改动**：
  - `OpenSpecTest/src/app/GameApp.h/.cpp`（拾取 API、Gizmo 模式、输入门控、Tick 集成）
  - `OpenSpecTest/src/ui/MainWindow.cpp`（`RenderViewportWidget` 鼠标事件）
  - `OpenSpecTest/src/ui/DetailPanelWidget.cpp`（同一 Actor Transform 外部变更时刷新控件）
  - `OpenSpecTest/src/ui/WorldContentPanelWidget.cpp`（视口选中后大纲高亮，若尚未实现则一并补齐）
  - `OpenSpecTest/src/render/Dx12Renderer.h/.cpp`（Gizmo 线段绘制，复用 Line PSO）
- **依赖与约束**：拾取矩阵须与 `Dx12Renderer::UpdateCameraConstants` 一致；Transform 读写统一走 `AActor::GetActorTransform` / `SetActorTransform`。
- **非目标（本变更不包含）**：GPU/ID Buffer 拾取、遮挡判定、多选、Undo/Redo、局部/世界空间 Gizmo 切换。
- **已实现但原提案未列**：三角面精确拾取、SAH BVH、性能菜单；仍不包含遮挡感知与多选。
