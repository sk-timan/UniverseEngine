## 目的

定义源网格文件与图片 Import 为引擎资产（`.uasset` + `.meta`）的流程与约束，统一 Import Factory 入口并支持 Reimport。

## 需求

### 需求:Import 必须生成 uasset 与 meta

系统必须在用户导入源网格文件时，将 Assimp 解析结果转换为引擎资产并写入磁盘；禁止仅创建内存对象而不持久化。

#### 场景:首次导入 StaticMesh

- **当** 用户选择 `Soldier.fbx` 并指定目标路径 `Meshes/Characters/Soldier`
- **那么** 系统必须在 `Content/Meshes/Characters/` 下创建 `Soldier.uasset` 与 `Soldier.uasset.meta`
- **那么** `Soldier.uasset` 的 header.type 必须为 `StaticMesh`
- **那么** meta 必须记录 source_file 与 import_settings

#### 场景:首次导入 SkeletalMesh

- **当** 用户选择含骨骼/蒙皮的 fbx/gltf 并指定目标路径
- **那么** 系统必须创建 type 为 `SkeletalMesh` 的 uasset 与 meta
- **那么** uasset.object 必须包含骨骼与蒙皮 Section 数据

#### 场景:Import 失败不得留下不完整 uasset

- **当** Assimp 解析或 Serialize 失败
- **那么** 系统必须删除或不写入半成品 uasset 文件
- **那么** 必须向用户返回可读错误信息

### 需求:Import Factory 必须包装 MeshImporter

系统必须通过 `UMeshImportFactory` 统一 Import 入口；禁止 Editor 菜单直接调用 `MeshImporter` 后跳过 Save。

#### 场景:ImportAndSave 原子操作

- **当** Editor 触发模型导入
- **那么** 系统必须依次执行 Import → 填充 PackageHeader → Serialize → 写 uasset/meta → 更新 AssetRegistry

#### 场景:StaticMesh Import 写入二进制 uasset

- **当** Import Factory 成功 Save StaticMesh
- **那么** 磁盘 uasset 必须为二进制容器（magic `UAST`）
- **那么** meta 仍必须为 JSON sidecar

### 需求:Reimport 必须保留资产身份

系统必须支持对已有 uasset 的 Reimport；Reimport 后 AssetPath 与 GUID 不变，几何数据更新。

#### 场景:源文件变更后 Reimport

- **当** 用户对已存在的 `Meshes/Characters/Soldier.uasset` 执行 Reimport 且源 fbx 已修改
- **那么** 系统必须保留 header.guid 与 asset_path
- **那么** 系统必须用新几何覆盖 uasset.object 并更新 meta.source_timestamp

#### 场景:Reimport 后运行时对象刷新

- **当** Reimport 成功且该资产已 Load 到 ResourceRegistry
- **那么** 系统必须使已 Load 对象重新 Deserialize 或替换为 Load 新数据后的实例
- **那么** 引用该 Mesh 的 Component 在下一帧必须使用更新后的几何

### 需求:Import 规范必须涵盖 Texture2D 类型

资产 Import 规范必须将 Texture 与 Mesh 并列，均要求 uasset + meta 持久化与 Factory 统一入口。

#### 场景:Content Browser 导入纹理

- **当** 用户在 Content Browser 选择导入图片并指定 Content 目标路径
- **那么** 系统必须调用 `UTextureImportFactory` 而非 `UMeshImportFactory`
- **那么** 生成的 uasset header.type 必须为 `Texture2D`

#### 场景:Import 后 Registry 立即可见

- **当** Texture Import Factory 成功写入 uasset
- **那么** AssetRegistry 必须立即索引该 Texture2D 条目，无需重启 Editor
