# editor-transform-gizmo

## 目的

在选中 Actor 上提供 UE5 风格 Transform Gizmo（平移/旋转/缩放），并与 Detail 面板、相机输入解耦。

## 需求

### 需求:选中 Actor 必须在 Root 世界坐标显示 Transform Gizmo
系统必须在存在有效选中 Actor 时，在其 Root 组件世界位置（无 Root 时使用 `ActorTransform.Position`）绘制 Transform Gizmo；系统必须在无选中时禁止绘制 Gizmo。

#### 场景:选中后显示 Gizmo
- **当** `GameApp::GetSelectedActorObjectId()` 非零且对应 Actor 存在于活动关卡
- **那么** 系统必须在下一帧渲染中于该 Actor 锚点位置绘制与当前 Gizmo 模式匹配的操控器线框

#### 场景:取消选中后隐藏 Gizmo
- **当** 当前选中被清除或选中 Actor 已被销毁
- **那么** 系统必须停止绘制 Transform Gizmo

### 需求:必须支持 UE5 风格的 W E R 模式切换
系统必须提供平移、旋转、缩放三种 Gizmo 模式，并通过键盘快捷键切换；系统必须使用边沿触发检测，禁止按住按键时重复切换。

#### 场景:W 切换到平移模式
- **当** 视口聚焦且未处于鼠标环视状态，用户按下 `W` 键（本帧按下、上一帧未按下）
- **那么** 系统必须将 Gizmo 模式设置为 Location（平移），并更新 Gizmo 可视形态

#### 场景:E 切换到旋转模式
- **当** 视口聚焦且未处于鼠标环视状态，用户按下 `E` 键（本帧按下、上一帧未按下）
- **那么** 系统必须将 Gizmo 模式设置为 Rotation（旋转），并更新 Gizmo 可视形态

#### 场景:R 切换到缩放模式
- **当** 视口聚焦且未处于鼠标环视状态，用户按下 `R` 键（本帧按下、上一帧未按下）
- **那么** 系统必须将 Gizmo 模式设置为 Scale（缩放），并更新 Gizmo 可视形态

### 需求:拖拽 Gizmo 必须修改 Actor Transform 并刷新场景
系统必须支持通过拖拽 Gizmo 轴或环修改选中 Actor 的 `FActorTransform`；系统必须在拖拽过程中持续更新视口渲染表现。

#### 场景:平移轴拖拽更新位置
- **当** 用户处于平移模式并在视口左键拖拽已命中的平移轴
- **那么** 系统必须按轴约束更新 `ActorTransform.Position`，并在拖拽过程中持续刷新渲染

#### 场景:旋转环拖拽更新旋转
- **当** 用户处于旋转模式并拖拽已命中的旋转环
- **那么** 系统必须更新 `ActorTransform.Rotation`（与项目 `FRotator3` / `FTransform` 约定一致），并刷新渲染

#### 场景:缩放轴拖拽更新缩放
- **当** 用户处于缩放模式并拖拽已命中的缩放轴
- **那么** 系统必须更新 `ActorTransform.Scale`，且每个分量不得低于配置的最小正值阈值

### 需求:Gizmo 与 Detail 面板必须保持 Transform 双向同步
系统必须使 Gizmo 拖拽结果反映到 Detail 面板 Transform 控件；Detail 面板手工修改也必须反映到 Gizmo 位置；系统必须防止 UI 回写触发无限更新循环。

#### 场景:Gizmo 拖拽结束后 Detail 与拾取缓存同步
- **当** 用户释放左键且释放前处于 Gizmo 拖拽状态
- **那么** 系统必须调用 `BumpSceneRevision()` 一次，使 Detail 面板与 Actor 级拾取 BVH 与新的 Transform 对齐

#### 场景:Detail 修改后 Gizmo 跟随
- **当** 用户在 Detail 面板提交 Transform 变更且当前存在选中 Actor
- **那么** 系统必须调用 `SetSelectedActorTransform(..., bBumpSceneRevision=true)`，并在下一帧将 Gizmo 绘制锚点与形态与新的 `ActorTransform` 对齐

#### 场景:拖拽中禁止无意义的场景全量刷新
- **当** Gizmo 拖拽计算得到的 Transform 与当前 Actor Transform 完全相同
- **那么** 系统不得调用 `SetSelectedActorTransform` 或 `BumpSceneRevision()`，禁止触发 `RefreshActiveLevelRender` 的重复 Present

### 需求:视口相机移动必须与 Gizmo 快捷键解耦
系统必须将 `W`/`E`/`R` 在非鼠标环视时专用于 Gizmo 模式；系统必须将 `WASD` 与 `QE` 相机移动限制在鼠标环视（右键按住）激活期间，以消除与 UE5 编辑器一致的快捷键冲突。

#### 场景:环视时 WASD 移动相机
- **当** 视口聚焦且鼠标环视处于激活状态
- **那么** 系统必须允许 `W`/`A`/`S`/`D`/`Q`/`E` 驱动相机移动与升降，且不得将 `W`/`E`/`R` 解释为 Gizmo 模式切换

#### 场景:非环视时 WER 不移动相机
- **当** 视口聚焦且鼠标环视未激活
- **那么** 系统必须禁止 `W`/`A`/`S`/`D`/`Q`/`E` 驱动相机平移，并将 `W`/`E`/`R` 用于 Gizmo 模式切换

### 需求:Gizmo 必须使用线段渲染且保持可读尺度
系统必须通过 `Dx12Renderer` 的线段绘制路径渲染 Gizmo；系统必须根据相机距离缩放 Gizmo 尺寸，使操控器在屏幕上保持近似恒定可读大小。

#### 场景:Gizmo 在场景之后绘制
- **当** 帧渲染包含场景网格与编辑器 Gizmo
- **那么** 系统必须在场景不透明绘制完成后再绘制 Gizmo 线段，并启用深度测试

### 需求:Gizmo 拖拽不得导致帧率腰斩式下降
系统必须保证 Gizmo 拖拽路径每帧仅执行一次完整场景 Present；系统禁止在 `SetSelectedActorTransform` 内调用 `RefreshActiveLevelRender`（含二次 Present）。

#### 场景:拖拽中由 Tick 统一渲染
- **当** 用户按住 Gizmo 轴拖拽且 Transform 发生变化
- **那么** 系统必须仅通过 `AActor::SetActorTransform` 更新数据，并由 `GameApp::Tick` 末尾的 `World::Render` + `Dx12Renderer::Render` 完成单帧呈现

#### 场景:快速连点 Gizmo 无移动时不触发 Transform 写入
- **当** 用户在 Gizmo 上快速连点且鼠标未移动（`mouseMovedSincePress == false`）
- **那么** 系统可以进入短暂拖拽态，但 `UpdateDrag` 无 Transform 变化时不得写回 Transform 或递增 `SceneRevision`
