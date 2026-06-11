# Texture 资产管线 — 技术设计

## 上下文

工程已具备：

- 资产继承：`UStreamableRenderAsset` → `UStaticMesh` / `USkeletalMesh`
- 磁盘管线：`UMeshImportFactory` → `UAssetSerializer`（`UAST` 二进制容器）→ `FAssetRegistry` → `UAssetManager`
- 渲染：DX12 `Dx12Renderer`，Mesh 侧 VB/IB 在 `CreateRenderState` 上传
- 第三方：`stb_image` / `stb_image_resize2` 已接入 CMake（`stb::stb`）

**缺口**：无 Texture 资产类；无图片 Import；uasset payload 无 Texture 格式；Renderer 无从 uasset 创建 `ID3D12Resource` SRV 的路径。

本设计对齐 UE 四层模型，在 **不重构 Mesh 管线** 的前提下扩展 Texture 分支。

**约束：**

- C++20、Allman 风格、UE 式命名（`F`/`U` 前缀）
- Editor Import 可读源文件；Runtime/Load **禁止**读源 PNG/TGA/EXR
- GPU 句柄不写入 uasset 磁盘
- MVP 仅 `UTexture2D`；Cook 在 Import 时同步完成（无独立 UnrealPak 打包步骤）

## 目标 / 非目标

**目标：**

- 类层次：`UStreamableRenderAsset` → `UTexture` → `UTexture2D`
- Import：PNG/TGA/JPEG/BMP → `UTexture2D` + `FTextureSource` → 写 `Content/Textures/.../*.uasset` + `.meta`
- Cook（Import 内）：由 Source 生成全 mip RGBA8 链 → `FTexturePlatformData` Bulk 写入 uasset payload
- Load：`UAssetManager::GetOrLoad` 反序列化 PlatformData；按需 `InitResource()` 创建 DX12 纹理 + SRV
- Reimport：保留 guid/asset_path，更新 Source 与 PlatformData
- Content Browser：Texture 缩略图基于 mip0 像素
- Assimp 内嵌贴图解码复用 `FImageDecoder::DecodeFromMemory`

**非目标：**

- `UTextureCube` / RenderTarget / Volume
- BC/DXT/BC7 块压缩（PlatformData 暂存未压缩 RGBA8 mip）
- 独立 Cook 命令行 / `.ucook` 发布包
- RenderThread 专用线程（Resource 仍在主线程或现有渲染路径 Init，与 Mesh GPU Upload 一致）
- 完整 Material 图与 PBR 采样（仅预留 `UTexture2D` 引用与 SRV 绑定 API）
- EXR 解码（Phase 2；stb 不支持 EXR，MVP 先 PNG/TGA/JPEG/BMP，HDR 可选 via `stbi_loadf`）

## 决策

### 决策 1：资产类继承

```
UObject
  └── UStreamableRenderAsset
        ├── UStaticMesh / USkeletalMesh   （已有）
        └── UTexture                      （抽象，不可实例化）
              └── UTexture2D              （MVP）
```

- **`UTexture`**：压缩设置、sRGB、Filter、AddressMode、Import 元数据接口；声明 `HasResidentPlatformData()` / `HasResidentTextureResource()`；禁止直接 Serialize 实例
- **`UTexture2D`**：尺寸、mip 数量、持有 `FTextureSource`（Editor 可选驻留）与 `FTexturePlatformData`；实现 Serialize/Deserialize

`UStreamableRenderAsset::HasResidentGeometryData()` **不**用于 Texture；Texture 使用独立查询，避免语义混淆。

### 决策 2：四层数据分离

| 层 | 类型 | 生命周期 | 内容 |
|----|------|----------|------|
| 资产对象 | `UTexture2D` | Game/Editor | 元数据、Import 设置、引用路径 |
| 源数据 | `FTextureSource` | Editor uasset / Reimport | 解码后 RGBA8（或 float 供 HDR）；对应 UE `FTextureSource` |
| 平台数据 | `FTexturePlatformData` | uasset Bulk | 各 mip 级 RGBA8 字节；对应 UE Cook 产物（未压缩版） |
| 运行时资源 | `FTextureResource` | GPU | `ID3D12Resource` + SRV；对应 `FTextureResource` / `FRHITexture` |

**Import 流程：**

```
源文件 → FImageDecoder (stb) → FTextureSource (内存)
       → BuildMips (CPU, 可选 stb_image_resize2 缩 MaxSize)
       → FTexturePlatformData
       → UAssetSerializer::Save (Source 摘要 + PlatformData Bulk)
```

**Load 流程：**

```
uasset → Deserialize PlatformData → UTexture2D
       → RequestLoad / InitResource → FTextureResource (Upload mips to DX12)
```

**Reimport：** 仅重跑 Import 管线；guid/asset_path 不变。

### 决策 3：uasset 格式 — Texture2D binary payload

沿用现有 `UAST` 容器；新增 `payload_format = 2`（`Texture2D binary`）。

**Payload 布局（v1）：**

```
[object_name: string]
[asset_path: string]
[width: u32][height: u32][mip_count: u32]
[format: u8]          // 0 = RGBA8 UNORM
[sRGB: u8]            // 0/1
[filter: u8][address_u: u8][address_v: u8]
[source_width: u32][source_height: u32][source_format: u8]
[source_blob_len: u32][source_blob: bytes]   // 可选；Editor 可省略以减小体积，meta 必留 source_file
[mip_count_platform: u32]
for each mip:
  [mip_width: u32][mip_height: u32][mip_data_len: u32][mip_data: bytes]
```

- **header.type**：`Texture2D`（Registry / AssetTypeInfo 一致）
- **meta.sidecar**：与 Mesh 相同字段 + `import_settings`（`sRGB`、`max_size`、`flip_y` 等）

**备选：** JSON payload — 拒绝；大图 Base64 体积与 Load 性能不可接受。

### 决策 4：Import Factory 与 ImageDecoder

| 类 | 职责 |
|----|------|
| `FImageDecoder` | 封装 stb：`LoadFromFile`、`LoadFromMemory`；统一输出 `FDecodedImage { Width, Height, Channels, bIsSRGB, RGBA8 }` |
| `UTextureImportFactory` | `ImportTexture2DAndSave`、`Reimport`；调 Decoder → BuildMips → 填充 `UTexture2D` → Serializer → Registry |
| `FTextureCookUtils` | CPU mip 生成、MaxSize 缩放（`stb_image_resize2`） |

- Windows 路径：`STBI_WINDOWS_UTF8`
- Assimp embedded：对 `aiTexture` 压缩 blob 调 `stbi_load_from_memory`

### 决策 5：GPU Resource 与 Renderer 衔接

- **`FTextureResource`** 持有 `ComPtr<ID3D12Resource>`、`D3D12_CPU_DESCRIPTOR_HANDLE` SRV（使用现有 CBV/SRV/UAV heap 分配策略或专用 Texture heap）
- **`UTexture2D::InitResource(Dx12Renderer*)`**：从 PlatformData 上传各 mip；设置 `bHasResidentTextureResource = true`
- **`ReleaseResource()`**：与 Mesh Release 对称，Level 切换或 Unload 时调用
- Material/Mesh 采样（后续）：通过 `GetShaderResourceView()` 绑定；本期仅实现 Resource 创建与单元/手动验证

Upload 格式：MVP 统一 `DXGI_FORMAT_R8G8B8A8_UNORM` 或 `_SRGB` 由 `bSRGB` 决定。

### 决策 6：目录与 SoftObjectPath

```
Content/
  Textures/<Category>/<Name>.uasset
  Textures/<Category>/<Name>.uasset.meta
```

- **虚拟路径示例**：`Textures/Characters/T_Diffuse.T_Diffuse`
- **depends_on**：Material uasset 未来引用 Texture 时写入 header（本期 Texture Import 可为空）

### 决策 7：StreamableRenderAsset 扩展

在 `UTexture` 中新增虚接口（不破坏 Mesh）：

```cpp
virtual bool HasResidentPlatformData() const;
virtual bool HasResidentTextureResource() const;
virtual void InitResource(class Dx12Renderer* InRenderer);
virtual void ReleaseResource();
```

`UStreamableRenderAsset` 基类保持现有 Mesh API；Texture  override 上述方法。

### 决策 8：EXR 与 HDR 策略

| 格式 | MVP | 方案 |
|------|-----|------|
| PNG/TGA/JPEG/BMP | ✅ | stb |
| HDR (.hdr) | 可选 | `stbi_loadf` → tonemap → RGBA8 |
| EXR | ❌ Phase 2 | 引入 `tinyexr` 或专用库；Import 对话框过滤提示 |

提案流程图含 EXR；实现分 Phase，spec 中 EXR 场景标记为 Phase 2。

## 风险 / 权衡

| 风险 | 缓解 |
|------|------|
| uasset 体积大（未压缩 RGBA8 + Source） | Import 设置 MaxSize；PlatformData 必存 mip；Source blob 可配置不嵌入 uasset，仅 meta.source_file |
| 主线程 Upload 大卡顿 | MVP 接受；后续 Async Load + 分 mip 上传 |
| EXR 用户预期 | Import UI 明确支持格式；EXR 报错并文档化 Phase 2 |
| SRV heap 耗尽 | 统计已创建 Texture 数；spec 限制单张最大尺寸 |
| flip Y 与 UV 不一致 | import_settings.flip_y + 文档；默认与 stb/DX12 约定一致 |

## 迁移计划

1. 落地类与 Serializer（无 Editor UI）→ 单元测试 Save/Load 往返
2. Import Factory + Content Browser Import 入口
3. GPU InitResource + 手动验证 SRV
4. TextureThumbnailProvider
5. （后续变更）Material 引用 Texture SoftObjectPath

无 Breaking 变更：现有关卡与 Mesh 不受影响。

## 待决问题

- Source blob 是否默认嵌入 uasset（便于无源文件 Reimport）还是仅依赖 meta.source_file？**倾向：meta 必存路径；uasset 内嵌 Source 可选，默认不嵌以控体积。**
- Cook 是否在 Editor 启动时对旧 uasset  lazy 重建？**倾向：否；仅 Import/Reimport 时 Cook。**
