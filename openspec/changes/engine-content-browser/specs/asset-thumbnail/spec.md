## 新增需求

### 需求:系统必须为可渲染资产生成预览缩略图

系统必须为可渲染目标类型的 uasset 生成预览缩略图，并在 Content Browser Tile 中展示。

#### 场景:StaticMesh 缩略图

- **当** Content Browser 展示 Type 为 StaticMesh 的资产
- **那么** 系统必须为该资产生成基于几何数据的预览缩略图（非纯文字占位）
- **那么** 缩略图必须在 Tile 的缩略图区域显示

#### 场景:SkeletalMesh 缩略图

- **当** Content Browser 展示 Type 为 SkeletalMesh 的资产
- **那么** 系统必须为该资产生成基于几何数据的预览缩略图

#### 场景:Texture 类型预留

- **当** Registry 条目 Type 为 Texture 且存在对应 uasset 实现与 Provider
- **那么** 系统必须生成基于纹理数据的预览缩略图
- **备注**: 本期 Texture uasset 类型尚未实现时，必须回退至默认缩略图

### 需求:非可渲染资产必须使用统一默认缩略图

对于不可渲染或尚未实现 Preview Provider 的资产类型，系统必须使用统一的默认缩略图。

#### 场景:未知类型

- **当** 资产 Type 不在可渲染类型集合中（如未来自定义 Actor 类型）
- **那么** Tile 缩略图区域必须显示统一默认图标
- **那么** 不得尝试 Load 完整 uasset payload 仅用于缩略图

#### 场景:可渲染类型 Provider 不可用

- **当** 资产 Type 标记为可渲染但无可用 Provider（如 Texture 尚未实现）
- **那么** 系统必须回退至统一默认缩略图

### 需求:缩略图生成必须异步且可缓存

缩略图生成不得阻塞 UI 主线程；系统必须缓存已生成结果以避免重复计算。

#### 场景:异步生成

- **当** 某资产 Tile 进入可见区域且尚无缓存缩略图
- **那么** 系统必须在后台线程或异步队列中生成缩略图
- **那么** 生成完成前 Tile 必须显示占位状态（灰块或加载指示）
- **那么** 生成完成后必须更新 Tile 显示

#### 场景:缓存命中

- **当** 同一资产（相同 guid 且 uasset 文件未修改）再次请求缩略图
- **那么** 系统必须直接返回缓存结果，不得重复 Load 与绘制

#### 场景:Reimport 后失效

- **当** 资产 Reimport 成功且 uasset 内容或修改时间变更
- **那么** 系统必须使该资产旧缩略图缓存失效并重新生成

### 需求:缩略图系统必须支持 Provider 扩展

系统必须通过 Provider 接口区分不同资产类型的缩略图生成逻辑，并预留后续 DX12 离屏渲染升级路径。

#### 场景:Provider 路由

- **当** 请求某 Registry 条目的缩略图
- **那么** 系统必须按 Type 选择第一个 CanProvide 为 true 的 Provider
- **那么** 若无 Provider 匹配，必须使用 DefaultThumbnailProvider

#### 场景:Phase 1 CPU 预览

- **当** MeshThumbnailProvider 处理 StaticMesh 或 SkeletalMesh
- **那么** 必须使用 CPU 几何数据与正交投影绘制预览（如 QPainter flat shading）
- **那么** 不得在本期要求 DX12 离屏 RenderTarget

#### 场景:Phase 2 扩展预留

- **当** 后续引入 Dx12OffscreenThumbnailProvider
- **那么** Provider 链必须允许其优先于 CPU Provider 处理可渲染类型，且不可用时 fallback CPU

### 需求:缩略图请求必须限制并发以保护性能

系统必须限制缩略图生成的并发与每帧完成数量，避免 Editor 卡顿。

#### 场景:可见性优先

- **当** 多个资产同时缺少缩略图
- **那么** 系统必须优先为当前可见 Tile 请求生成

#### 场景:每帧预算

- **当** 异步队列中有待处理缩略图任务
- **那么** 每帧应用到 UI 的完成数量必须有上限（如 2），避免单帧大量 pixmap 更新
