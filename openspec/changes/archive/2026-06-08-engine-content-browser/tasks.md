## 1. 数据层 — Registry 修复与工具类

- [x] 1.1 修改 `ScanContentDirectory`：循环内改用 `RegisterFromDisk` 替代 `RegisterScannedUAsset`
- [x] 1.2 为 `FAssetRegistry` 新增 `GetRevision()`，在 scan/register upsert 时递增
- [x] 1.3 实现 `AssetFolderTree`：从 `ListAssets()` 按 AssetPath 分段构建文件夹树
- [x] 1.4 实现 `AssetTypeInfo`：Type → displayName、accentColor 静态映射（含 Unknown/预留 Texture）

## 2. 缩略图 — Phase 1 CPU Provider

- [x] 2.1 定义 `IAssetThumbnailProvider` 接口与 `IsRenderableAssetType` 判定
- [x] 2.2 实现 `FAssetThumbnailService`：LRU 缓存（guid + mtime）、异步队列、revision 失效
- [x] 2.3 实现 `MeshThumbnailProvider`：LoadObject → CPU 正交投影 → QPainter 绘制 128×128
- [x] 2.4 实现 `DefaultThumbnailProvider`：统一默认图标
- [x] 2.5 接入可见 Tile 优先与每帧完成上限（如 2）

## 3. UI 组件 — Content Browser 面板

- [x] 3.1 实现 `AssetListModel`（QAbstractListModel，SoftObjectPath/Type/UserRole）
- [x] 3.2 实现 `AssetTileDelegate`：UE 风格 Tile（缩略图 + 色条 + 双行文字）
- [x] 3.3 实现 `AssetBrowserPanelWidget`：QSplitter 左树右格、搜索框、类型 ComboBox、状态栏
- [x] 3.4 实现文件夹选中 + 搜索 + 类型三重过滤逻辑
- [x] 3.5 基于 Registry revision 的 `RefreshFromRegistry()` 脏检测刷新

## 4. 交互 — 拖拽、右键、视口接收

- [x] 4.1 Grid 拖拽：MIME `application/x-universeengine-softobjectpath`
- [x] 4.2 `RenderViewportWidget`/`ViewportHostWidget` 接收 drop → `LoadModelToActiveLevel`
- [x] 4.3 非 mesh 类型 drop 拒绝并提示
- [x] 4.4 右键菜单：资源管理器中显示、Reimport、复制 SoftObjectPath

## 5. MainWindow 集成与样式

- [x] 5.1 `BuildAssetBrowserPanel()`：底部 `QDockWidget` "Content Browser"
- [x] 5.2 Import 成功后调用 `RefreshFromRegistry()`
- [x] 5.3 扩展 `EditorStyle.cpp`：AssetBrowser 相关 QSS（Grid、SearchEdit）

## 6. 验证

- [x] 6.1 手动验证：启动后左树/右格展示全部 uasset，Type 与 header 一致
- [x] 6.2 手动验证：StaticMesh/SkeletalMesh 出现 CPU 预览缩略图；未知类型为默认图
- [x] 6.3 手动验证：搜索与类型过滤、拖拽 mesh 到视口生成 Actor
- [x] 6.4 手动验证：右键 Reimport 后 guid 不变、缩略图与 Tile 刷新
- [x] 6.5 手动验证：Import 新资产后浏览器自动出现新 Tile
