## 目的

定义 Texture2D uasset 反序列化、PlatformData 驻留与 GPU Resource 按需 Upload 的 Load 路径约束；运行时 Load 禁止读取源图片文件。

## 需求

### 需求:AssetManager 必须 Load Texture2D uasset

系统必须通过 `UAssetManager::GetOrLoad` 加载 `Texture2D` 类型 uasset；Load 路径禁止读取源 PNG/TGA/JPEG 文件。

#### 场景:Load Texture2D uasset

- **当** 请求 Load `Textures/Characters/T_Diffuse.T_Diffuse`
- **那么** 系统必须从 `Content/Textures/Characters/T_Diffuse.uasset` 反序列化
- **那么** 必须返回有效的 `UTexture2D` 且 `HasResidentPlatformData()` 为 true
- **那么** 禁止读取 meta 中 source_file 指向的磁盘图片

#### 场景:Load 失败返回错误

- **当** Texture2D uasset 不存在或 payload_format 无效
- **那么** 系统必须返回明确错误信息
- **那么** 禁止静默回退到 stb 读源文件

### 需求:Texture2D uasset 必须使用二进制 payload

系统必须在 Save Texture2D 时写入二进制 Texture payload；Load 时必须解析 Bulk mip 数据，禁止对大纹理构建完整 JSON DOM。

#### 场景:新 Import 写入二进制 Texture uasset

- **当** Import Factory 成功 Save Texture2D
- **那么** 磁盘文件必须以 magic `UAST` 开头
- **那么** payload_format 必须为 2（Texture2D binary）
- **那么** header 仍为 JSON，且 guid/asset_path/type 字段正确

#### 场景:Texture2D 往返一致性

- **当** 对含 width/height/mip_count 与 PlatformData 的 UTexture2D 执行 Save 再 Load
- **那么** Load 后的尺寸、mip 级数与各 mip 字节长度必须与 Save 前一致

### 需求:Load 后必须按需创建 GPU Texture Resource

uasset 不得包含 GPU 纹理句柄；Load 后由 `UTexture2D::InitResource` 上传 DX12。

#### 场景:Load 不立即创建 GPU 纹理

- **当** AssetManager 完成 Texture2D uasset Load
- **那么** `HasResidentTextureResource()` 必须为 false，直至 InitResource 成功

#### 场景:InitResource 上传 SRV

- **当** 调用 `UTexture2D::InitResource` 且 PlatformData 有效
- **那么** 系统必须创建 `ID3D12Resource` 与可用 SRV
- **那么** `HasResidentTextureResource()` 必须为 true

#### 场景:ReleaseResource 释放 GPU

- **当** 调用 `UTexture2D::ReleaseResource` 或 Asset Unload
- **那么** 系统必须释放 GPU 纹理与 SRV 占用
- **那么** `HasResidentTextureResource()` 必须为 false
- **那么** PlatformData CPU 数据可仍驻留（除非显式 Unload 对象）

### 需求:Load 后必须注册到 ResourceRegistry

系统必须在成功 Load Texture2D 后将资产注册到 `ResourceRegistry`，供后续 Material/Mesh 引用。

#### 场景:GetOrLoad 缓存 Texture

- **当** 同一 Texture SoftObjectPath 第二次调用 GetOrLoad
- **那么** 系统必须返回已 Load 的实例，禁止重复读盘（除非显式 Reload/Reimport）
