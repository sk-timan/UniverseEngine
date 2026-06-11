# Texture 资产管线（UTexture2D + Import / Cook / Load / GPU）

## 为什么

网格资产管线（`StaticMesh` / `SkeletalMesh` → uasset → Load）已落地，但 **Texture 仅在 `AssetTypeInfo` 中预留类型**，尚无 `UTexture` 类、Import Factory、uasset 持久化或 GPU 上传路径。Material 与 Mesh 的贴图引用无法走引擎资产流程，Content Browser 的 Texture 缩略图也只能回退默认图。

需要在现有 `UStreamableRenderAsset` 体系下补齐 UE 风格的 Texture 分层（Source → PlatformData → Resource），使 PNG/TGA/EXR 等源文件可 Import、Reimport、Load 并在渲染管线中采样。

## 变更内容

- 新增资产类：`UTexture`（抽象基类）→ `UTexture2D`（MVP 唯一具象类型）
- 新增数据层：`FTextureSource`（Editor/Reimport 源像素）、`FTexturePlatformData`（Cook 后 mip Bulk）、`FTextureResource`（DX12 GPU 纹理）
- 新增 `UTextureImportFactory` + `FImageDecoder`（基于已接入的 `stb_image`）统一 Import/Reimport 入口
- 扩展 `UAssetSerializer` / `AssetManager` 支持 `Texture2D` uasset 二进制 payload 的 Save/Load
- Import 时同步执行 **轻量 Cook**（生成 mip 链、写入 PlatformData Bulk；MVP 不做 BC 块压缩）
- Load 后按需创建 `FTextureResource` 并上传 DX12；为 Material/Mesh 采样预留绑定接口
- Content Browser 增加 `TextureThumbnailProvider`
- **非目标（本期）**：`UTextureCube`、Virtual Texture、异步流式 mip、Material 完整着色器采样实现、Assimp 批量抽贴图自动化

## 功能 (Capabilities)

### 新增功能

- `texture-import`：外部图片 Import 为 `Texture2D` uasset + meta；含 `FTextureSource` 持久化与 Reimport
- `texture-load`：`UTexture2D` uasset Load、PlatformData 解析、GPU Resource 创建与 Resident 状态查询

### 修改功能

- `asset-import`：资产 Import 规范扩展至 Texture 类型（与 Mesh Import Factory 并列）
- `asset-load`：AssetManager Load 路径扩展支持 `Texture2D`；运行时禁止读源 PNG/TGA/EXR
- `asset-registry`：Registry 索引与按类型过滤支持 `Texture2D`
- `asset-thumbnail`：Texture 类型启用基于像素数据的缩略图 Provider（替换默认图回退）

## 影响

- **新增**：`UTexture` / `UTexture2D`、`FTextureSource`、`FTexturePlatformData`、`FTextureResource`、`UTextureImportFactory`、`FImageDecoder`、`TextureThumbnailProvider`
- **修改**：`UAssetSerializer`、`UAssetManager`、`FAssetRegistry`、`AssetTypeInfo`（`Texture2D` 类型字符串）、Content Browser Import 菜单、`StreamableRenderAsset`（Texture 侧 Resident 查询扩展点）
- **依赖**：已有 `thirdparty/stb`（PNG/JPEG/TGA/BMP/HDR）；EXR 需 Phase 2 引入专用解码或延后
- **不变**：Mesh Import 流程、Component/Actor 体系、Level 序列化（Material 引用扩展留后续变更）
