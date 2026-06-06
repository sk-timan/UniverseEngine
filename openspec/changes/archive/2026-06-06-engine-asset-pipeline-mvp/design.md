# 引擎资产管线 MVP — 技术设计

## 上下文

工程已具备 UE 风格的 Component/Asset 分层（`UStreamableRenderAsset` → `UStaticMesh` / `USkeletalMesh`），以及 `MeshImporter`（Assimp）、`ResourceRegistry`、`Level` JSON 序列化。当前缺口是**磁盘侧的引擎资产层**：导入结果仅驻留内存，Level 加载时通过 `ResolveAssetImportPath` + Assimp 回退重建几何。

本设计在**不重构 Component/Actor 体系**的前提下，引入 `.uasset` 持久化与 AssetManager 加载路径，对齐 `docs/rendering-architecture-design.md` 第 4、5 节的 Import→持久化→Load 流程。

**约束：**

- C++20、Qt6 Editor、DX12 渲染、现有 `nlohmann::json` 序列化
- `Content/` 与现有 `OpenSpecTest/data/` 并存（gameplay/关卡 JSON 仍用 data）
- 第一期 uasset 为**二进制容器 v1**（StaticMesh POD payload + JSON header）；`.uasset.meta` 仍为 JSON
- GPU VB/IB 不写入磁盘
- 异步 Import 进度条、启动预缓存 uasset（后续优化，非 MVP 阻塞）

## 目标 / 非目标

**目标：**

- 源文件（fbx/gltf/obj）Import 后生成 `Content/**/*.uasset` + `.uasset.meta`
- 运行时/Level 加载仅通过 AssetManager 读 uasset，禁止 Assimp
- `FAssetRegistry` 扫描 Content 建立 Editor 索引
- Reimport 保留 AssetPath/GUID，覆盖几何数据
- Component/Level 引用 `SoftObjectPath`（如 `Meshes/Characters/Soldier.Soldier`）

**非目标：**

- 二进制 uasset v1（StaticMesh POD）；SkeletalMesh 仍走 JSON payload
- Cook/`.ucook` 发布流水线
- 完整 Content Browser UI（仅 Registry + 现有导入入口）
- Material/Texture/动画序列资产
- RenderThread/SceneProxy/流式加载

## 决策

### 决策 1：uasset 格式 — 二进制容器 v1 + JSON Header

- **选择**：磁盘文件为二进制容器；顶层结构 = magic + version + 长度前缀 JSON header + payload。header 仍含 magic（`UEAS`）、version、type、guid、asset_path 等 JSON 字段；**StaticMesh 几何走 POD 二进制 payload**（顶点/索引/Section 直接 memcpy），SkeletalMesh 与其它类型 payload 仍为 JSON 文本。
- **原因**：实测大模型 JSON 解析占 Load 耗时 ~83%（14MB uasset 约 7.5s）；二进制 payload 可降至毫秒级。header 保持 JSON 便于 Registry 索引与调试；Load 仍向后兼容旧版纯 JSON uasset。
- **备选**：纯 JSON v1 — 大 mesh 不可接受；全二进制 header — Registry 扫描成本上升。

**磁盘布局（v1 二进制容器）：**

```
[magic: "UAST" (4B)]
[container_version: u32 = 1]
[header_json_len: u32]
[header_json: UTF-8 JSON，字段同 PackageHeader v1]
[payload_format: u8 — 0=JSON text, 1=StaticMesh binary]
[payload: 格式相关]
```

**StaticMesh binary payload：** object_name、asset_path（长度前缀字符串）→ vertex_count + FVertex[] → index_count + uint32[] → section_count + Section[]（含 bounds）。

**向后兼容：** 无 `UAST` magic 的文件按旧版 `{header, object}` JSON 解析。

**PackageHeader 字段（v1）：**

| 字段 | 类型 | 说明 |
|------|------|------|
| magic | string | `"UEAS"` |
| version | int | `1` |
| type | string | `StaticMesh` / `SkeletalMesh` |
| guid | string | UUID v4，Reimport 不变 |
| asset_path | string | 虚拟路径，如 `Meshes/Characters/Soldier` |
| object_name | string | 主对象名，如 `Soldier` |
| depends_on | string[] | 依赖资产路径，第一期可为空 |

**`.uasset.meta`（sidecar，JSON）：**

| 字段 | 说明 |
|------|------|
| source_file | 源 fbx/gltf 路径（相对 Content/Source 或 data/models） |
| source_timestamp | 源文件最后修改时间 |
| import_settings | Import 参数 JSON（坐标转换、合并选项等） |
| import_hash | source + settings 哈希，用于检测 Reimport 必要性 |

### 决策 2：目录布局

```
Content/
  Source/                          # 可选：导入前源文件副本
  Meshes/<Category>/<Name>.uasset
  Meshes/<Category>/<Name>.uasset.meta
```

- **Content 根**：`GProjectContentDirectory`，默认 `{ProjectRoot}/Content`
- **磁盘路径**：`Content/Meshes/Characters/Soldier.uasset`
- **虚拟路径 / SoftObjectPath**：`Meshes/Characters/Soldier.Soldier`（不含 `Content/` 前缀）

### 决策 3：单 uasset 单主对象

- **选择**：一个 `.uasset` 仅含一个主 `UStaticMesh` 或 `USkeletalMesh`；Inner/Outer 子对象后续扩展。
- **原因**：与 UE 常见「一资产一 Primary Object」一致，Registry 与 Load 逻辑简单。

### 决策 4：模块边界

```
UMeshImportFactory
  └─ MeshImporter (Assimp) → UStaticMesh/USkeletalMesh
  └─ UAssetSerializer::Save → .uasset + .meta
  └─ FAssetRegistry::RegisterAsset

UAssetManager::GetOrLoad(SoftObjectPath)
  └─ FAssetRegistry 查索引
  └─ UAssetSerializer::Load → UObject
  └─ ResourceRegistry::RegisterAsset
  └─ （渲染层按需 Upload GPU）

Editor / Level
  └─ Import 菜单 → Factory
  └─ Level Load → AssetManager（禁止 Assimp 回退）
```

| 类 | 职责 |
|----|------|
| `FAssetPackageHeader` | header 读写、校验 magic/version |
| `UAssetSerializer` | Save/Load uasset；调用 `UStaticMesh::Serialize` / Deserialize |
| `FAssetRegistry` | 启动/Import 后扫描 `Content/**/*.uasset`；Path→Type→Source |
| `UMeshImportFactory` | ImportAndSave、Reimport |
| `UAssetManager` | GetOrLoad、Unload、内存缓存；对接 `ResourceRegistry` |

### 决策 5：GPU 数据不序列化

- **选择**：uasset 只存 CPU 侧顶点/索引/Section/Bounds/骨骼数据；Load 后由现有 Dx12 路径创建 VB/IB（与 Component `CreateRenderState` 衔接）。
- **原因**：避免 uasset 体积膨胀与 GPU 平台耦合；与现有 `FStaticMeshSection::VertexBufferGPU` 生命周期一致。

### 决策 6：引用与序列化字段变更

- **ComponentSerializer** mesh 字段由 `mesh_asset_id`（源路径）改为 `mesh_asset_path`（SoftObjectPath）
- **Hard Reference**：Load Level 时必须 Load 对应 uasset
- **GUID**：Reimport/覆盖 uasset 时 header.guid 不变；重命名资产文件时生成新 guid（视为新资产）

### 决策 7：废弃路径

- 移除 `ULevel::ResolveAssetImportPath` 的 Assimp 回退及 `bNeedsImport`
- 迁移 `Dx12Renderer` 内 `aiImportFile` 至 uasset Load（地图模型改为引用 uasset）

## 风险 / 权衡

| 风险 | 缓解 |
|------|------|
| JSON uasset 体积大、Load 慢 | StaticMesh 改用二进制 POD payload；旧 JSON uasset 仍可 Load，建议 Reimport 升级 |
| 现有关卡引用 fbx 路径 | Migration：加载旧关卡时提示重新 Import 或一次性迁移工具 |
| GUID/重命名混乱 | spec 定义：Reimport 保 guid；移动/重命名 = 新资产 |
| Deserialize 不完整 | Phase 1 单元测试 StaticMesh/SkeletalMesh 往返 |
| Editor 无 Content Browser | Registry 提供 `ListAssets()` 供后续 UI；Import 对话框指定目标路径 |

## 迁移计划

1. **Phase 1**：实现 Serializer + Save/Load 单测（不接入 Level）
2. **Phase 2**：Import 菜单走 Factory，新导入资产写 uasset
3. **Phase 3**：Level/Component 改引用；移除 Assimp 回退
4. **Phase 4**：旧关卡手动 Re-import 或写一次性迁移脚本（非 MVP 阻塞）
5. **回滚**：保留 MeshImporter 代码路径 feature flag（开发期），默认关闭 Assimp 回退

## 待决问题

- Import 时是否**复制**源文件到 `Content/Source/`（建议：记录路径即可，第一期不强制复制）
- `GProjectContentDirectory` 是否写入 CMake/GameApp 配置（建议：与 `GProjectDataDirectory` 并列）
- Reimport UI 入口：Import 对话框增加「覆盖已有 uasset」选项，或独立菜单项
