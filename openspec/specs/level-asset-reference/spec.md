## 目的

定义 Level/Component 对 uasset 的引用方式与加载约束；Load 路径禁止 Assimp 回退。

## 需求

### 需求:Component 必须引用 SoftObjectPath 而非源文件路径

Mesh 组件序列化必须存储 uasset 虚拟路径（SoftObjectPath），禁止存储 fbx/gltf 源文件路径作为 Load 依据。

#### 场景:StaticMeshComponent 序列化

- **当** StaticMeshComponent 引用 `Meshes/Characters/Soldier` 资产
- **那么** Level JSON 中 mesh 引用字段必须为 `Meshes/Characters/Soldier.Soldier` 形式
- **那么** 禁止将 `data/models/.../Soldier.fbx` 写入该字段

#### 场景:反序列化后 Resolve 资产

- **当** Level 加载含 mesh_asset_path 的 Component 数据
- **那么** 系统必须通过 AssetManager::GetOrLoad 解析为 UStaticMesh 或 USkeletalMesh
- **那么** 必须调用 Component SetStaticMesh / SetSkeletalMesh 完成绑定

### 需求:Level 加载禁止 Assimp 回退

Level 加载路径不得因「无 resident 几何」而调用 MeshImporter 或 Assimp。

#### 场景:加载已保存关卡

- **当** 用户加载含 mesh_asset_path 的 Level JSON
- **那么** 系统必须仅通过 uasset Load 获取 Mesh 数据
- **那么** 禁止执行 `ULevel::ResolveAssetImportPath` 的 Assimp 导入回退逻辑

#### 场景:uasset 缺失时失败

- **当** Level 引用的 uasset 不存在
- **那么** 系统必须报告错误并跳过或中止该 Component 的 Mesh 绑定
- **那么** 禁止尝试从 fbx 自动 Import

#### 场景:禁止 Assimp Load 路径（验收）

- **当** Level 加载且 uasset 存在
- **那么** 系统禁止调用 `MeshImporter::ImportStaticMesh` 或 `ImportSkeletalMesh` 作为 Load 路径

### 需求:Import 到关卡必须与 uasset 绑定

Editor「导入模型到关卡」必须先确保 uasset 存在（Import 或 Load），再 Spawn Actor 引用该 uasset。

#### 场景:导入新模型到关卡

- **当** 用户通过「导入」菜单从源文件生成 uasset
- **那么** 系统必须先 ImportAndSave 生成 uasset
- **那么** 不得直接 Spawn Actor（Spawn 由「加载模型」完成）

#### 场景:从 uasset 加载模型到关卡

- **当** 用户通过「加载模型」选择已有 `.uasset`
- **那么** 系统必须通过 AssetManager::GetOrLoad 加载
- **那么** Spawn 的 Actor/Component 必须引用该 uasset 的 SoftObjectPath

### 需求:重启后 Level 加载必须无需源文件

在 uasset 已存在的前提下，重启 Editor 加载 Level 必须仅依赖 uasset，不要求源 fbx 在线。

#### 场景:重启持久化验证

- **当** 用户 Import 并保存 Level 后关闭 Editor 再启动
- **那么** 加载 Level 后 Mesh 必须正确显示
- **那么** Load 过程不得访问 source_file（meta 中记录的路径）
