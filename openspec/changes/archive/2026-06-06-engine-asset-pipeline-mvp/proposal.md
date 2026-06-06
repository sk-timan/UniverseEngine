# 引擎资产管线 MVP（方案 A：UAsset 最小可用化）

## 为什么

当前网格资源在运行时通过 Assimp 直接读取 fbx/gltf，Level 加载时还会再次导入；`UStaticMesh` / `USkeletalMesh` 缺少持久化的引擎资产文件，导致无法像 UE5 一样统一管理、Reimport 与 Editor 浏览。现有 Component/Asset 骨架已就绪，现在需要补上「源文件 → `.uasset` → Load」中间层，消除运行时对 Assimp 的依赖。

## 变更内容

- 引入 `Content/` 目录与 `.uasset`（二进制容器 + JSON header v1）作为引擎资产持久化格式
- 新增 Import Factory、AssetSerializer、AssetRegistry、AssetManager 模块
- 导入 fbx/gltf 时生成 `.uasset` + `.uasset.meta`（记录 SourceReference、ImportSettings）
- Component/Level 序列化改为引用 `SoftObjectPath`（如 `Meshes/Characters/Soldier.Soldier`），不再引用源文件路径
- Level 加载与运行时通过 AssetManager Load uasset，**不再调用 Assimp**
- 支持 Reimport：源文件变更后覆盖 uasset，保留 AssetPath/GUID
- **BREAKING**：现有关卡中存 fbx 相对路径的 `MeshAssetId` 需迁移为 uasset 路径；Level 加载不再 Assimp 回退

## 功能 (Capabilities)

### 新增功能

- `asset-import`：源文件 Import 为 uasset + meta，含 Import Factory 与 Save 流程
- `asset-load`：uasset 反序列化为 `UStaticMesh` / `USkeletalMesh`，AssetManager GetOrLoad，GPU Upload 衔接
- `asset-registry`：扫描 `Content/**/*.uasset` 建立索引（Path、Type、SourceFile、Dependencies）
- `level-asset-reference`：Level/Component 引用 uasset 路径，加载关卡时经 AssetManager 解析，禁止 Assimp 回退

### 修改功能

- （无）— 主规范目录中尚无 level-persistence 规范；本变更通过 `level-asset-reference` 新规范定义关卡对资产的引用方式，实现阶段需同步修改 Level 序列化与加载逻辑

## 影响

- **新增**：`Content/` 目录约定、`FAssetPackageHeader`、`UAssetSerializer`、`FAssetRegistry`、`UAssetManager`、`UMeshImportFactory`
- **修改**：`MeshImporter` 调用方、`ULevel` Import/Load、`ComponentSerializer` mesh 引用字段、`MainWindow` 导入流程、`ResourceLoader`（可选薄封装）
- **废弃**：`ULevel` 中 `bNeedsImport` + Assimp 回退；`Dx12Renderer` 内直接 Assimp 路径
- **不变**：Component/Actor 继承体系、`ResourceRegistry` 运行时缓存、现有 Editor 视口与 Gizmo 行为
