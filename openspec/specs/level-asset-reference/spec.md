## 目的

定义 Level/Component 对 uasset 的引用方式、解析顺序与加载约束；Load 路径禁止 Assimp 回退。引用必须能在资产重命名/移动后仍可解析（Guid 主引用 + Redirect 双保险）。

## 需求

### 需求:Component 必须引用 SoftObjectPath 而非源文件路径

Mesh 组件序列化必须存储 uasset 虚拟路径（SoftObjectPath），禁止存储 fbx/gltf 源文件路径作为 Load 依据。

#### 场景:StaticMeshComponent 序列化

- **当** StaticMeshComponent 引用 `Meshes/Characters/Soldier` 资产
- **那么** Level JSON 中 mesh 引用字段必须为 `Meshes/Characters/Soldier.Soldier` 形式
- **那么** 禁止将 `data/models/.../Soldier.fbx` 写入该字段

#### 场景:反序列化后 Resolve 资产

- **当** Level 加载含 `mesh_asset_path` 与/或 `mesh_asset_guid` 的 Component 数据
- **那么** 系统必须通过 `FAssetReferenceResolver::Resolve` 解析有效 SoftObjectPath
- **那么** 系统必须通过 `AssetManager::GetOrLoad` 加载为 `UStaticMesh` 或 `USkeletalMesh`
- **那么** 必须调用 Component `SetStaticMesh` / `SetSkeletalMesh` 完成绑定
- **那么** 解析成功后必须回写 Component 的 Guid 与当前 SoftObjectPath，供后续保存迁移

### 需求:Component 必须序列化 mesh_asset_guid 作为稳定引用

Mesh 组件除 SoftObjectPath 外，必须持久化 uasset header 中的稳定 `guid`，作为资产重命名后的主解析依据。

#### 场景:保存时写入 Guid 与当前路径

- **当** 用户保存含 Mesh 组件的 Level JSON
- **那么** 每个 Mesh 组件必须写入 `mesh_asset_guid`（非空时）
- **那么** 必须同时写入 `mesh_asset_path`（与 `mesh_asset_id` 兼容字段，值为当前 SoftObjectPath）
- **那么** 保存前必须通过 `FAssetReferenceResolver` 将路径解析为 Registry 中的最新 SoftObjectPath

#### 场景:加载时 Guid 优先解析

- **当** Level JSON 含非空 `mesh_asset_guid`
- **那么** 系统必须优先通过 `FAssetRegistry::FindByGuid` 定位资产
- **那么** 命中后必须使用 Registry 中的 `asset_path` 与 `object_name` 构造 SoftObjectPath
- **那么** 不得因 `mesh_asset_path` 过期而导致 Load 失败

#### 场景:运行时回填 Guid

- **当** Component 仅有 SoftObjectPath 且 Registry 可查到对应条目
- **那么** `SetMeshAssetId` 或序列化逻辑必须尽可能回填 `mesh_asset_guid`

### 需求:Level 加载必须通过 FAssetReferenceResolver 解析引用

系统必须使用统一的引用解析器，在 Guid、Redirect、Registry 与路径候选之间按固定优先级解析 Mesh 引用。

#### 场景:解析优先级

- **当** 调用 `FAssetReferenceResolver::Resolve(InGuid, InSoftObjectPath)`
- **那么** 若 `InGuid` 非空且 Registry 命中，必须返回该条目对应的 SoftObjectPath（`bResolvedByGuid=true`）
- **那么** 否则必须依次尝试：原始 SoftObjectPath、规范化 SoftObjectPath、Redirect 链结果、Redirect 后的规范化路径
- **那么** 若候选 SoftPath 在 Registry 命中，必须返回 Registry 中的规范 SoftObjectPath 与 Guid
- **那么** 若仅 `asset_path` 命中（对象名变更），允许通过 `FindByAssetPath` 回退匹配并返回当前 ObjectName
- **那么** 若 Registry 均未命中，必须返回 Redirect 后的 SoftObjectPath 供 `GetOrLoad` 文件路径回退

#### 场景:AssetManager Load 前解析 Redirect

- **当** 调用 `UAssetManager::GetOrLoad(SoftObjectPath)`
- **那么** 系统必须先经 `FAssetReferenceResolver` 解析 Redirect 与 Registry
- **那么** 必须使用解析后的有效 SoftObjectPath 定位 uasset 文件

#### 场景:Editor 启动时加载 Redirect 表

- **当** Editor 启动并执行 `FAssetRegistry::ScanContentDirectory`
- **那么** 系统必须从 `Content/AssetRedirects.json` 加载 redirect 映射到内存
- **那么** redirect 文件不存在时必须安全降级为空映射，不得阻止启动

### 需求:资产重命名必须记录 Redirect 以修复旧路径引用

Content Browser 或资产重命名流程在变更 SoftObjectPath 后，必须写入持久化 redirect，使仅保存旧路径的关卡仍可加载。

#### 场景:重命名对象或移动路径后记录 Redirect

- **当** `FAssetRenameService` 成功执行 `RenameAssetObject` 或 `RelocateAssetPath`
- **那么** 系统必须记录 `旧 SoftObjectPath → 新 SoftObjectPath` 到 `FAssetRedirectStore`
- **那么** 必须持久化到 `Content/AssetRedirects.json`
- **那么** uasset header 中的 `guid` 必须保持不变

#### 场景:文件夹重命名级联 Redirect

- **当** 用户重命名 Content 文件夹且其下多个 uasset 被 Relocate
- **那么** 每个受影响资产必须各自写入一条 redirect
- **那么** 旧关卡中仍引用旧 SoftObjectPath 的 Component 必须能通过 Redirect 链解析到新路径

#### 场景:Redirect 链式解析

- **当** `A → B` 与 `B → C` 均存在于 redirect 表
- **那么** 对 `A` 的解析必须返回 `C`
- **那么** 链式解析必须有最大深度保护，防止循环引用

### 需求:Level 加载禁止 Assimp 回退

Level 加载路径不得因「无 resident 几何」而调用 MeshImporter 或 Assimp。

#### 场景:加载已保存关卡

- **当** 用户加载含 `mesh_asset_path` 的 Level JSON
- **那么** 系统必须仅通过 uasset Load 获取 Mesh 数据
- **那么** 禁止执行 `ULevel::ResolveAssetImportPath` 的 Assimp 导入回退逻辑

#### 场景:uasset 缺失时失败

- **当** Level 引用的 uasset 不存在且 Redirect/Guid 均无法解析到有效资产
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
- **那么** Spawn 的 Actor/Component 必须引用该 uasset 的 SoftObjectPath 与 Guid

### 需求:重启后 Level 加载必须无需源文件

在 uasset 已存在的前提下，重启 Editor 加载 Level 必须仅依赖 uasset，不要求源 fbx 在线。

#### 场景:重启持久化验证

- **当** 用户 Import 并保存 Level 后关闭 Editor 再启动
- **那么** 加载 Level 后 Mesh 必须正确显示
- **那么** Load 过程不得访问 source_file（meta 中记录的路径）

#### 场景:重命名资产后加载已保存关卡

- **当** 用户保存 Level 后，在 Content Browser 重命名或移动被引用的 uasset，再重新打开该 Level
- **那么** Mesh 必须仍能正确显示
- **那么** 若 Level 含 `mesh_asset_guid`，必须优先通过 Guid 解析成功
- **那么** 若 Level 仅含旧 `mesh_asset_path`，必须能通过 Redirect 或 Registry 回退解析成功

### 需求:旧关卡引用迁移约束

系统必须明确区分可自动修复与必须人工迁移的旧关卡引用格式。

#### 场景:仅含旧 SoftObjectPath 的关卡可部分自动修复

- **当** 旧 Level JSON 仅含 `mesh_asset_path`/`mesh_asset_id`（SoftObjectPath 格式），且无 `mesh_asset_guid`
- **那么** 若路径在 `AssetRedirects.json` 中有记录，或 Registry 可按路径/AssetPath 匹配，系统必须自动加载
- **那么** 加载成功后应回写 Guid 与最新 SoftObjectPath；用户再次保存即可完成迁移

#### 场景:仍引用源文件路径的关卡不可自动修复

- **当** 旧 Level JSON 的 mesh 字段仍为 `data/models/.../*.fbx`、`.glb` 等源文件路径
- **那么** 系统不得尝试 Assimp 回退加载
- **那么** 用户必须在 Editor 中重新 Import/加载模型到关卡并保存，以写入 SoftObjectPath 与 Guid

#### 场景:迁移完成后的目标格式

- **当** 用户保存经修复或重新放置模型后的 Level
- **那么** JSON 必须同时包含非空 `mesh_asset_guid`（若 Registry 可提供）与当前 `mesh_asset_path`
- **那么** 后续资产重命名必须仅依赖 Guid 与 Redirect 即可保持引用有效
