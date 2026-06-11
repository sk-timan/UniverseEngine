## MODIFIED Requirements

### 需求:系统必须为可渲染资产生成预览缩略图

系统必须为可渲染目标类型的 uasset 生成预览缩略图，并在 Content Browser Tile 中展示。

#### 场景:StaticMesh 缩略图

- **当** Content Browser 展示 Type 为 StaticMesh 的资产
- **那么** 系统必须为该资产生成基于几何数据的预览缩略图（非纯文字占位）
- **那么** 缩略图必须在 Tile 的缩略图区域显示

#### 场景:SkeletalMesh 缩略图

- **当** Content Browser 展示 Type 为 SkeletalMesh 的资产
- **那么** 系统必须为该资产生成基于几何数据的预览缩略图

#### 场景:Texture2D 缩略图

- **当** Registry 条目 Type 为 Texture2D 且 uasset 已成功 Import
- **那么** 系统必须生成基于纹理 mip0 像素数据的预览缩略图
- **那么** 必须使用 TextureThumbnailProvider 而非 DefaultThumbnailProvider
- **那么** 禁止在 Texture2D 已可 Load 时仍回退至默认缩略图

#### 场景:Texture 类型 Load 失败时回退

- **当** Registry 条目 Type 为 Texture2D 但 uasset Load 或 payload 解析失败
- **那么** 系统必须回退至统一默认缩略图
- **那么** 不得阻塞 Content Browser 刷新
