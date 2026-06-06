# engine-asset-pipeline-mvp — 收尾说明

## 旧关卡 mesh 引用迁移

**BREAKING：** 关卡 JSON 中组件的 `mesh_asset_id` 若仍为源文件绝对路径（例如 `.fbx`），Load 时将无法解析。

### 新格式

- 组件序列化字段：`mesh_asset_path`（SoftObjectPath，例如 `Meshes/Imported/Soldier.Soldier`）
- 磁盘资产：`Content/<asset_path>.uasset` + 同名 `.uasset.meta`

### 迁移步骤

1. 在 Editor 中对每个旧模型重新 **Import**（或指定 Content 路径后 Import）。
2. Import 会写入 uasset/meta 并在关卡中保存 `mesh_asset_path`。
3. 保存关卡 JSON 后，重启 Editor 验证 Load 不再触发 Assimp。

### 地图模型

- 地图 FBX 首次 Load 时自动 Import 到 `Content/Meshes/Maps/<map_name>.uasset`。
- 地图 Actor 通过 `UAssetManager::GetOrLoad` 加载；不再由 `Dx12Renderer` 直接 Assimp 加载。

### Reimport

- `UMeshImportFactory::Reimport` 保留原 `guid` 与 `asset_path`，仅更新 object 与 meta；Reimport 后 StaticMesh uasset 必须为二进制格式。

### uasset 格式（二进制 v1）

- **新 Import / Reimport** 的 StaticMesh 写入二进制容器（magic `UAST` + JSON header + POD payload）。
- **旧 JSON uasset** 仍可 Load；建议 Reimport 一次以升级格式并提升 Load 性能。
- **SkeletalMesh** 当前仍使用 JSON payload（容器内 payload_format=0），后续可扩展二进制。

### Editor 工作流

- **导入**：源文件 → ImportAndSave → 仅写 uasset/meta，不 Spawn 到关卡。
- **加载模型**：选择已有 `.uasset` → GetOrLoad → Spawn 到关卡。
