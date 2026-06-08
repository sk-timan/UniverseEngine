## 修改需求

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

## 新增需求

### 需求:AssetRegistry 必须提供 revision 供 UI 脏检测

系统必须暴露 Registry 内容变更的版本计数，供 Editor UI 判断是否需要刷新。

#### 场景:revision 递增

- **当** 执行 ScanContentDirectory 或 RegisterFromHeader/RegisterFromDisk 成功 upsert 条目
- **那么** Registry revision 必须递增

#### 场景:UI 脏检测

- **当** Content Browser 记录的 revision 小于 Registry 当前 revision
- **那么** UI 必须重建文件夹树与资产列表（或执行等效增量刷新）
