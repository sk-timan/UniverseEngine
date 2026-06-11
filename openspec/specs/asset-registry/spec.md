## 目的

定义 Content 目录 uasset 扫描与内存索引，供 Editor 查询、Import 后注册及 Reimport 检测。

## 需求

### 需求:AssetRegistry 必须扫描 Content 目录

系统必须在 Editor 启动时（及 Import/Reimport 后）扫描 `Content/**/*.uasset` 并建立内存索引。

#### 场景:启动扫描

- **当** Editor 启动且 `Content/` 目录存在
- **那么** 系统必须遍历所有 `.uasset` 并解析 header（至少 asset_path、type、guid）
- **那么** 必须在内存中建立 Path → 条目 的映射
- **那么** 扫描实现必须调用 RegisterFromDisk（或等价的 LoadHeader + RegisterFromHeader 路径），禁止仅按文件路径推导而跳过 header 解析

#### 场景:Import 后增量注册

- **当** Import Factory 成功写入新 uasset
- **那么** AssetRegistry 必须立即索引该资产，无需重启 Editor

#### 场景:header 解析失败降级

- **当** 某 uasset 文件 header 解析失败
- **那么** Registry 仍必须索引该文件（至少 AssetPath、ObjectName 来自路径推导）
- **那么** Type/Guid 可为空，但不得阻止该文件出现在 ListAssets 全量列表中

### 需求:AssetRegistry 必须提供 revision 供 UI 脏检测

系统必须暴露 Registry 内容变更的版本计数，供 Editor UI 判断是否需要刷新。

#### 场景:revision 递增

- **当** 执行 ScanContentDirectory 或 RegisterFromHeader/RegisterFromDisk 成功 upsert 条目
- **那么** Registry revision 必须递增

#### 场景:UI 脏检测

- **当** Content Browser 记录的 revision 小于 Registry 当前 revision
- **那么** UI 必须重建文件夹树与资产列表（或执行等效增量刷新）

### 需求:Registry 条目必须包含 Source 与 Type 信息

每个索引条目必须支持 Editor 查询资产类型与源文件路径。

#### 场景:按类型列出资产

- **当** 调用 ListAssets 并过滤 type=`StaticMesh`
- **那么** 系统必须仅返回 StaticMesh 类型 uasset 的路径列表

#### 场景:按类型列出纹理

- **当** 调用 ListAssets 并过滤 type=`Texture2D`
- **那么** 系统必须仅返回 Texture2D 类型 uasset 的路径列表

#### 场景:Texture 条目 Source 信息

- **当** Registry 索引 Texture2D uasset
- **那么** 条目必须可读 meta.source_file 供 Reimport 与 Content Browser 展示

#### 场景:depends_on 预留 Material 引用

- **当** Texture uasset header 含 depends_on
- **那么** Registry 必须原样暴露 depends_on 列表（可为空）

#### 场景:查询 SourceReference

- **当** 查询 `Meshes/Characters/Soldier` 的 Registry 条目
- **那么** 必须返回对应 `.uasset.meta` 中的 source_file 路径（若 meta 存在）

### 需求:Registry 必须解析 meta 侧车文件

系统必须读取 `.uasset.meta` 获取 source_file、import_hash，用于 Reimport 检测。

#### 场景:meta 缺失时降级

- **当** uasset 存在但 meta 缺失
- **那么** Registry 仍必须索引该 uasset（header 字段）
- **那么** source_file 字段可为空，Reimport 功能必须提示用户重新指定源文件

### 需求:Registry 必须预留依赖字段

索引条目必须包含 depends_on 列表（来自 uasset header），供后续 Material/Texture 引用扩展。

#### 场景:无依赖资产

- **当** uasset header.depends_on 为空数组
- **那么** Registry 条目 depends_on 必须为空，Load 时不得尝试 Load 其他资产
