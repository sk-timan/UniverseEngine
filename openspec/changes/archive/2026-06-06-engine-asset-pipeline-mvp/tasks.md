## 1. 数据层 — Package 格式与序列化



- [x] 1.1 定义 `FAssetPackageHeader` 与 JSON schema v1（magic、version、type、guid、asset_path、depends_on）

- [x] 1.2 实现 `UAssetSerializer::Save` / `Load` 读写 `.uasset` 文件

- [x] 1.3 实现 `.uasset.meta` 读写（source_file、source_timestamp、import_settings、import_hash）

- [x] 1.4 扩展 `UStaticMesh::Serialize` 与 `Deserialize`，覆盖顶点/索引/Section/Bounds 完整往返

- [x] 1.5 扩展 `USkeletalMesh::Serialize` 与 `Deserialize`，覆盖骨骼与蒙皮 Section 往返

- [x] 1.6 添加 `GProjectContentDirectory` 全局路径（与 `GProjectDataDirectory` 并列初始化）

- [x] 1.7 编写 Serializer 单元测试或独立测试可执行文件验证 StaticMesh/SkeletalMesh 往返

- [x] 1.8 实现二进制 uasset 容器（magic `UAST`、StaticMesh POD payload、JSON header）

- [x] 1.9 Load 路径向后兼容旧版纯 JSON uasset

- [x] 1.10 Import/Load 菜单拆分（「导入」写 uasset、「加载模型」读 uasset 到关卡）



## 2. 管线层 — Registry、Manager、Import Factory



- [x] 2.1 实现 `FAssetRegistry`：扫描 `Content/**/*.uasset`、解析 header、读取 meta

- [x] 2.2 实现 `FAssetRegistry::RegisterAsset` / `ListAssets(type)` / `FindByPath`

- [x] 2.3 实现 `UAssetManager::GetOrLoad(SoftObjectPath)`，对接 `ResourceRegistry`

- [x] 2.4 实现 `UAssetManager::Unload` 与 Load 缓存策略

- [x] 2.5 实现 `UMeshImportFactory::ImportAndSave`（包装 `MeshImporter` + Serializer + Registry 注册）

- [x] 2.6 实现 `UMeshImportFactory::Reimport`（保留 guid/asset_path，覆盖 object 与 meta）

- [x] 2.7 Import 失败时清理半成品 uasset 的逻辑



## 3. Editor 与 Level 接入



- [x] 3.1 修改 `MainWindow` / `GameApp::ImportModelToActiveLevel`：Import 前先 ImportAndSave uasset

- [x] 3.2 Import 对话框增加目标 Content 路径输入（默认 `Meshes/<Category>/<Name>`）

- [x] 3.3 修改 `ComponentSerializer`：mesh 字段改为 `mesh_asset_path`（SoftObjectPath）

- [x] 3.4 修改 `ULevel` Load：通过 `AssetManager::GetOrLoad` 绑定 Mesh，移除 Assimp 回退

- [x] 3.5 移除或废弃 `ULevel::ResolveAssetImportPath` 的 Import 用途及 `bNeedsImport` 逻辑

- [x] 3.6 修改 `ULevel::ImportModelFromFile`：Spawn Actor 引用 uasset 而非直接 Import 到内存

- [x] 3.7 Editor 启动时调用 `FAssetRegistry::ScanContentDirectory`

- [x] 3.8 迁移 `Dx12Renderer` 内直接 Assimp 地图加载至 uasset Load（或预生成 map uasset）



## 4. 验证与收尾

详细步骤见 [ACCEPTANCE.md](./ACCEPTANCE.md)。



- [x] 4.1 手动验证：Import fbx → 磁盘存在 uasset/meta → 重启 Editor → Load 关卡显示正确

- [x] 4.2 手动验证：Level 加载过程无 Assimp 调用（日志或断点确认）

- [x] 4.3 手动验证：Reimport 后 guid 不变、几何更新、视口 Mesh 刷新

- [x] 4.4 手动验证：新 Import 的 StaticMesh uasset 文件头为 `UAST` magic，体积显著小于旧 JSON uasset

- [x] 4.5 手动验证：二进制 uasset Load 耗时可感知（大模型 Load < 2s，对比旧 JSON 数秒级）

- [x] 4.6 手动验证：旧版纯 JSON uasset 仍可 Load 且几何正确（向后兼容）

- [x] 4.7 手动验证：「导入」与「加载模型」分离流程 — Import 仅写盘，Load 从 uasset 进关卡

- [x] 4.8 文档：在变更 WRAPUP 或注释中记录旧关卡 mesh 引用迁移说明

- [x] 4.9 更新 `.gitignore` 白名单：允许 `Content/` 下 uasset 与 meta 提交（排除 Source 大文件可选）

