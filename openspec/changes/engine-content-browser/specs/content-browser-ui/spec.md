## 新增需求

### 需求:Editor 必须提供 Content Browser 面板

系统必须在 Editor 中提供 Content Browser 面板，用于浏览 `Content/` 下全部已注册 uasset 资产。

#### 场景:面板布局

- **当** Editor 启动且 Content Browser 面板可见
- **那么** 面板必须使用底部 Dock 呈现
- **那么** 面板左侧必须显示文件夹目录树，右侧必须显示资产 Tile 网格
- **那么** 布局必须支持水平分割调整树与网格宽度

#### 场景:展示全部 uasset

- **当** Content 目录下存在多个 `.uasset` 文件且已被 AssetRegistry 索引
- **那么** 选中根节点或 `Content` 时，右侧网格必须列出所有已注册资产
- **那么** 每个 Tile 必须显示资产名称与类型名称

### 需求:文件夹树必须反映虚拟 Content 路径

Content Browser 文件夹树必须基于资产的虚拟 AssetPath 构建，而非仅反映磁盘空目录。

#### 场景:按路径分层

- **当** Registry 中存在 `Meshes/Characters/Soldier` 与 `Meshes/Props/Box` 两条资产
- **那么** 文件夹树必须包含 `Content` → `Meshes` → `Characters`、`Props` 层级
- **那么** 叶子文件夹下不得出现 uasset 文件节点（资产仅出现在右侧网格）

#### 场景:选中文件夹过滤资产

- **当** 用户选中文件夹 `Meshes/Characters`
- **那么** 右侧网格必须仅显示 AssetPath 位于该文件夹下的资产（含直接子路径，不含其它分支）

### 需求:资产 Tile 必须采用 UE 风格视觉

资产网格中的每个 Tile 必须包含缩略图、类型色条与文字标签，视觉风格对齐 UE5 Content Browser。

#### 场景:Tile 组成

- **当** 网格渲染某一资产 Tile
- **那么** Tile 上方必须显示缩略图区域
- **那么** 缩略图下方必须显示一条与资产类型对应的颜色条
- **那么** 颜色条下方必须显示资产 ObjectName（主标签）与类型显示名（副标签）

#### 场景:底部状态栏

- **当** 当前过滤结果包含 N 个资产
- **那么** 面板底部必须显示 `N items` 或等价计数

### 需求:Content Browser 必须支持搜索与类型过滤

系统必须允许用户通过搜索框与类型过滤器缩小资产列表。

#### 场景:名称搜索

- **当** 用户在搜索框输入文本
- **那么** 网格必须仅显示 ObjectName 或 AssetPath 包含该文本（不区分大小写）的资产
- **那么** 搜索必须与当前选中文件夹取交集

#### 场景:类型过滤

- **当** 用户选择类型过滤器（如 StaticMesh）
- **那么** 网格必须仅显示 Registry 条目中 Type 匹配该类型的资产
- **那么** 类型过滤必须与搜索及文件夹选中取交集

### 需求:Content Browser 必须支持拖拽 mesh 到视口

系统必须允许用户将 mesh 类型资产从 Content Browser 拖拽到渲染视口以生成关卡 Actor。

#### 场景:发起拖拽

- **当** 用户在网格中对 StaticMesh 或 SkeletalMesh 资产发起拖拽
- **那么** 系统必须在拖拽数据中携带该资产的 SoftObjectPath

#### 场景:视口接收 drop

- **当** 用户将 mesh 资产拖放到渲染视口并释放
- **那么** 系统必须调用现有 Load/Spawn 流程，在活跃关卡中生成引用该 uasset 的 Actor
- **那么** 生成的 Actor 必须使用默认 Transform（Identity 或引擎约定的放置 Transform）

#### 场景:非 mesh 资产不可 drop

- **当** 用户尝试将非 mesh 类型资产拖放到视口
- **那么** 系统必须拒绝 drop 或向用户提示该类型不支持拖放

### 需求:Content Browser 必须提供右键上下文菜单

系统必须为网格中的资产提供右键菜单，支持常用 Editor 操作。

#### 场景:在资源管理器中显示

- **当** 用户右键选择「在资源管理器中显示」
- **那么** 系统必须打开该 uasset 文件所在目录

#### 场景:Reimport

- **当** 用户对具有 SourceFile 的 mesh 资产选择 Reimport
- **那么** 系统必须调用 UMeshImportFactory::Reimport
- **那么** Reimport 成功后 Content Browser 必须刷新该资产条目与缩略图

#### 场景:复制 SoftObjectPath

- **当** 用户选择复制 SoftObjectPath
- **那么** 系统必须将该资产的 SoftObjectPath 字符串写入系统剪贴板

### 需求:Content Browser 必须在资产变更后自动刷新

Content Browser 必须在 Registry 索引变更后更新文件夹树与资产网格，无需重启 Editor。

#### 场景:Import 后刷新

- **当** 用户通过 Import 菜单成功写入新 uasset
- **那么** Content Browser 必须显示新资产 Tile

#### 场景:启动扫描后展示

- **当** Editor 启动完成 Content 目录扫描
- **那么** Content Browser 必须展示扫描到的全部资产
