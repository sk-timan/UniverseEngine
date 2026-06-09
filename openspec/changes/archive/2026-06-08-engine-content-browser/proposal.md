# UE5 风格资产浏览器（Content Browser）

## 为什么

资产管线 MVP 已提供 `FAssetRegistry` 扫描、`UAssetManager` 加载与 `.uasset` 持久化，但 Editor 仍只能通过文件对话框选路径导入/加载，无法在编辑器内浏览 `Content/` 下全部资产。这与 UE5 Content Browser 的工作流差距明显，也阻碍了后续 Reimport、拖拽放置等编辑体验。在数据层就绪后，现在需要补上可视化浏览 UI 及缩略图预览能力。

## 变更内容

- 新增底部 Dock「Content Browser」面板：左侧文件夹树 + 右侧 UE 风格资产 Tile 网格
- 展示 `Content/**/*.uasset` 全部已注册资产，按虚拟路径目录分层
- 可渲染类型（`StaticMesh`、`SkeletalMesh`；预留 `Texture`）生成预览缩略图；其它类型（含未来自定义 Actor）使用统一默认缩略图
- 缩略图分阶段实现：第一期 CPU 正交投影预览 + 异步缓存；预留 DX12 离屏 Provider 接口
- 支持搜索/类型过滤、拖拽 mesh 资产到视口生成 Actor、右键菜单（资源管理器/Reimport/复制路径）
- 修复 `ScanContentDirectory` 启动扫描不解析 header 导致 `Type`/`Guid` 为空的问题
- **不在本期**：双击加载、Texture/Material uasset 类型、DX12 离屏缩略图、资产重命名/删除

## 功能 (Capabilities)

### 新增功能

- `content-browser-ui`：Editor Content Browser 面板（文件夹树、资产网格、搜索过滤、拖拽、右键菜单）
- `asset-thumbnail`：资产缩略图生成与缓存（Provider 扩展、可渲染类型判定、默认图回退）

### 修改功能

- `asset-registry`：扫描时必须解析 uasset header 填充 Type/Guid；新增 revision 供 UI 脏检测

## 影响

- **新增**：`AssetBrowserPanelWidget`、`AssetListModel`、`AssetTileDelegate`、`AssetFolderTree`、`AssetTypeInfo`、`AssetThumbnailService` 及 CPU Provider
- **修改**：`FAssetRegistry`（扫描逻辑、GetRevision）、`MainWindow`（底部 Dock、Import 后刷新）、`RenderViewportWidget`/`ViewportHostWidget`（拖拽接收）、`EditorStyle`（浏览器 QSS）
- **依赖**：现有 `FAssetRegistry`、`UAssetManager`、`UMeshImportFactory::Reimport`、`ULevel::SpawnModelFromSoftPath`
- **不变**：uasset 磁盘格式、Component/Actor 体系、主视口 DX12 渲染管线（Phase 2 离屏除外）
