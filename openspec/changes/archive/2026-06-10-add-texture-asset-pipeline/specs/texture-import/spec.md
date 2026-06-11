## ADDED Requirements

### 需求:Texture Import 必须生成 Texture2D uasset 与 meta

系统必须在用户导入外部图片文件时，将解码结果转换为 `UTexture2D` 引擎资产并写入磁盘；禁止仅创建内存对象而不持久化。

#### 场景:首次导入 PNG

- **当** 用户选择 `T_Diffuse.png` 并指定目标路径 `Textures/Characters/T_Diffuse`
- **那么** 系统必须在 `Content/Textures/Characters/` 下创建 `T_Diffuse.uasset` 与 `T_Diffuse.uasset.meta`
- **那么** `T_Diffuse.uasset` 的 header.type 必须为 `Texture2D`
- **那么** meta 必须记录 source_file 与 import_settings

#### 场景:首次导入 TGA

- **当** 用户选择 `.tga` 源文件并指定目标 Content 路径
- **那么** 系统必须创建 type 为 `Texture2D` 的 uasset 与 meta
- **那么** uasset payload 必须包含 Cook 后的 PlatformData mip Bulk

#### 场景:Import 失败不得留下不完整 uasset

- **当** 图片解码、Cook 或 Serialize 失败
- **那么** 系统必须删除或不写入半成品 uasset 文件
- **那么** 必须向用户返回可读错误信息

### 需求:Texture Import 必须通过 UTextureImportFactory 统一入口

系统必须通过 `UTextureImportFactory` 统一 Texture Import 入口；禁止 Editor 直接调用 `FImageDecoder` 后跳过 Save。

#### 场景:ImportAndSave 原子操作

- **当** Editor 触发纹理导入
- **那么** 系统必须依次执行 Decode → 填充 FTextureSource → Cook PlatformData → 填充 PackageHeader → Serialize → 写 uasset/meta → 更新 AssetRegistry

#### 场景:Texture2D Import 写入二进制 uasset

- **当** Import Factory 成功 Save Texture2D
- **那么** 磁盘 uasset 必须为二进制容器（magic `UAST`）
- **那么** payload_format 必须为 Texture2D binary（值为 2）
- **那么** meta 仍必须为 JSON sidecar

### 需求:Texture Reimport 必须保留资产身份

系统必须支持对已有 Texture2D uasset 的 Reimport；Reimport 后 AssetPath 与 GUID 不变，PlatformData 更新。

#### 场景:源文件变更后 Reimport

- **当** 用户对已存在的 `Textures/Characters/T_Diffuse.uasset` 执行 Reimport 且源 png 已修改
- **那么** 系统必须保留 header.guid 与 asset_path
- **那么** 系统必须用新 PlatformData 覆盖 uasset payload 并更新 meta.source_timestamp

#### 场景:Reimport 后缩略图失效

- **当** Texture Reimport 成功
- **那么** 系统必须使该资产缩略图缓存失效

### 需求:Texture Import 必须使用 FImageDecoder 解码

系统必须通过 `FImageDecoder`（基于 stb）解码 PNG/TGA/JPEG/BMP；禁止在 Import 路径手写格式解析。

#### 场景:解码为 RGBA8

- **当** Import 任意支持的 LDR 图片
- **那么** Decoder 必须输出 RGBA8 像素与正确的 width/height
- **那么** import_settings 必须记录 sRGB 意图

#### 场景:Assimp 内嵌贴图复用 Decoder

- **当** 未来从 `aiTexture` 压缩 blob 导入
- **那么** 系统必须调用 `FImageDecoder::DecodeFromMemory` 而非重复实现

#### 场景:不支持的 EXR 格式

- **当** 用户选择 `.exr` 文件且工程尚未启用 EXR 解码器
- **那么** 系统必须拒绝 Import 并返回明确错误
- **那么** 禁止写入部分 uasset
