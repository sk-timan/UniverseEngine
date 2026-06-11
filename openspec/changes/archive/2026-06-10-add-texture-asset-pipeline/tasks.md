## 1. 资产类与数据层



- [x] 1.1 新增 `UTexture` 抽象基类（Import 设置、sRGB、Filter、HasResidentPlatformData/HasResidentTextureResource、InitResource/ReleaseResource 虚接口）

- [x] 1.2 新增 `UTexture2D`，持有尺寸、mip 数、`FTextureSource`、`FTexturePlatformData`

- [x] 1.3 实现 `FTextureSource`（RGBA8 像素、尺寸、格式枚举）与 `FTexturePlatformData`（mip 数组 Bulk）

- [x] 1.4 实现 `FTextureResource`（DX12 Resource + SRV 句柄，不含序列化）

- [x] 1.5 更新 `AssetTypeInfo`：`Texture2D` 显示名与可渲染判定



## 2. 图片解码与 Cook



- [x] 2.1 实现 `FImageDecoder`（`ImageDecoder.cpp` 内 `STB_IMAGE_IMPLEMENTATION` + `STBI_WINDOWS_UTF8`）

- [x] 2.2 支持 PNG/TGA/JPEG/BMP 解码为 RGBA8；失败返回可读错误

- [x] 2.3 实现 `FTextureCookUtils`：MaxSize 缩放（stb_image_resize2）、CPU mip 链生成

- [x] 2.4 EXR 导入返回明确不支持错误（Phase 2 预留接口）



## 3. uasset 序列化



- [x] 3.1 在 `AssetSerializer` 定义 `payload_format = 2`（Texture2D binary）与读写布局

- [x] 3.2 实现 `UTexture2D` Serialize/Deserialize 与 `LoadObject` type 路由（`Texture2D`）

- [x] 3.3 编写 `OpenSpecTest_TextureSerializerTests` 验证 Save/Load 往返（尺寸、mip 数、像素 hash）



## 4. Import Factory 与 Registry



- [x] 4.1 实现 `UTextureImportFactory::ImportTexture2DAndSave`（Decode → Cook → Save → Register）

- [x] 4.2 实现 `UTextureImportFactory::Reimport`（保留 guid/asset_path，更新 payload 与 meta）

- [x] 4.3 Import 失败时清理半成品 uasset

- [x] 4.4 扩展 `FAssetRegistry::ListAssets` 过滤 `Texture2D`；Import 后增量注册



## 5. Load 与 GPU Resource



- [x] 5.1 扩展 `UAssetManager::GetOrLoad` 支持 Texture2D 并注册 ResourceRegistry

- [x] 5.2 实现 `UTexture2D::InitResource` / `ReleaseResource`（上传 mip 至 DX12、创建 SRV）

- [x] 5.3 手动或测试验证：Load → InitResource 后 `HasResidentTextureResource()==true`



## 6. Editor 与缩略图



- [x] 6.1 Content Browser / GameApp 增加「导入纹理」入口，调用 `UTextureImportFactory`

- [x] 6.2 Import 对话框：目标 Content 路径、import_settings（sRGB、max_size、flip_y）

- [x] 6.3 实现 `TextureThumbnailProvider`（Load mip0 或读 payload 缩略图 → QImage）

- [x] 6.4 注册 Provider 至 `FAssetThumbnailService`；Reimport 后缓存失效



## 7. 验证与收尾



- [x] 7.1 手动验证：Import png → 磁盘 uasset/meta → Registry 可见 → 缩略图正确

- [x] 7.2 手动验证：Reimport 后 guid 不变、缩略图更新

- [x] 7.3 手动验证：GetOrLoad Texture 不读取源 png 文件（日志/断点）

- [x] 7.4 运行全部相关单元测试通过

