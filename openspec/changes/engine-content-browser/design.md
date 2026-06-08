# UE5 风格资产浏览器 — 技术设计

## 上下文

资产管线 MVP（`engine-asset-pipeline-mvp`）已实现 `FAssetRegistry`、`UAssetManager`、`.uasset` 序列化与 Import/Reimport API。Editor 启动时调用 `ScanContentDirectory()`，Import 后通过 `RegisterFromHeader` 增量注册。但 **Editor UI 层无 Content Browser**，用户只能通过菜单 + `QFileDialog` 操作资产。

**已知实现缺口：** `ScanContentDirectory` 当前调用 `RegisterScannedUAsset`，仅从文件路径推导 `AssetPath`/`ObjectName`，**不解析 uasset header**，导致冷启动后 `Type`/`Guid`/`DependsOn` 为空，`ListAssets("StaticMesh")` 与缩略图类型路由失效。`RegisterFromDisk` 已具备正确逻辑，扫描路径未使用。

**约束：**

- C++20、Qt6 Widgets、DX12 主视口渲染
- 不修改 uasset 磁盘格式、Component/Actor 体系
- 缩略图与主视口 GPU 资源隔离，避免影响帧率
- 参考 UE5 Content Browser 布局：左树右格、Tile 卡片、类型色条

## 目标 / 非目标

**目标：**

- 底部 Dock「Content Browser」：左侧文件夹树 + 右侧 UE 风格资产 Tile 网格
- 展示 `Content/**/*.uasset` 全部已注册资产
- 可渲染类型（StaticMesh、SkeletalMesh；预留 Texture）生成预览缩略图；其它类型统一默认缩略图
- 搜索/类型过滤、拖拽 mesh 到视口、右键菜单（Explorer/Reimport/复制路径）
- 修复 Registry 扫描解析 header；新增 revision 供 UI 脏检测
- 缩略图 Phase 1：CPU 正交投影 + 异步缓存；预留 DX12 离屏 Provider 接口

**非目标：**

- 双击加载到关卡（用户未选）
- Texture/Material uasset 类型实现
- DX12 离屏缩略图（Phase 2）
- 资产重命名/删除、依赖图、Favorites、拖拽到文件夹
- 修改 uasset 格式或 Level 引用方式

## 决策

### 决策 1：UI 布局 — 底部 Dock + QSplitter 左树右格

- **选择**：`QDockWidget` 置于 `Qt::BottomDockWidgetArea`；内部 `QSplitter` 水平分割 `QTreeWidget`（~220px）与 `QListView`（IconMode + 自定义 Delegate）
- **原因**：对齐 UE5 Content Browser 习惯；底部全宽便于浏览大量资产；与现有 `WorldContentPanelWidget`（右侧 Outliner）职责分离
- **备选**：左侧 Dock — 与 Editor 面板争抢空间；独立窗口 — 增加管理成本

**Tile 结构（`AssetTileDelegate`）：**

```
┌─────────────┐
│  128×128    │  缩略图区（异步加载，占位灰块）
│  thumbnail  │
├─────────────┤  2px 类型色条
│ ObjectName  │  主标签（亮）
│ TypeName    │  副标签（暗）
└─────────────┘
```

### 决策 2：文件夹树 — 从 Registry 虚拟路径构建

- **选择**：`AssetFolderTree` 读取 `FAssetRegistry::ListAssets()`，按 `AssetPath` 的 `/` 分段建树；虚拟根 `All` → `Content` → 子文件夹
- **过滤规则**：选中 `Meshes/Characters` 时，资产 `AssetPath` 必须满足 `starts_with("Meshes/Characters/")` 或等于 `Meshes/Characters`（精确前缀边界，避免 `Meshes/Character` 误匹配）
- **原因**：不依赖磁盘空文件夹；与 UE 虚拟 Content 路径一致
- **备选**：`QFileSystemModel` — 无法展示仅存在于 Registry 的路径差异；无法合并 header Type 信息

### 决策 3：Registry 扫描修复 + Revision

- **选择**：`ScanContentDirectory` 循环内改为 `RegisterFromDisk`；新增 `uint64_t GetRevision()`，每次 scan/register/upsert 递增
- **原因**：满足主规范「启动扫描必须解析 header」；UI 通过 revision 对比避免每帧重建树/网格
- **备选**：UI 轮询文件系统 mtime — 无法感知 Import 后内存索引变化

### 决策 4：缩略图 Phase 1 — CPU 正交预览 + Provider 链

- **选择**：`FAssetThumbnailService` 单例 + `IAssetThumbnailProvider` 接口；Phase 1 实现 `MeshThumbnailProvider`（`UAssetSerializer::LoadObject` → CPU 顶点正交投影 → `QPainter` flat shading + 边线）与 `DefaultThumbnailProvider`
- **可渲染判定**：`StaticMesh`、`SkeletalMesh`、`Texture`（Texture 类型尚未实现 uasset，provider 暂不可用，回退默认图）
- **缓存**：内存 LRU，key = `guid + file_mtime`；Import/Reimport 后按 revision/guid 失效
- **异步**：`QThreadPool` 或 `std::async` 生成，主线程回调更新 Model；可见 Tile 优先；每帧最多完成 N 个（如 2）
- **原因**：无需 DX12 离屏 RT 与 GPU 同步，实现快、与主视口隔离；质量足够 MVP
- **备选**：Phase 1 直接 DX12 离屏 — 需共享 Device/CommandQueue、RT 生命周期与 Qt 纹理上传，成本高

**Phase 2 预留：** `Dx12OffscreenThumbnailProvider` 注册到 Provider 链高位，不可用时 fallback CPU。

### 决策 5：资产类型展示 — 静态映射表

- **选择**：`AssetTypeInfo` 映射 `Type string → { displayName, accentColor }`

| Type | 显示名 | 色条 |
|------|--------|------|
| StaticMesh | Static Mesh | `#5bc0de` |
| SkeletalMesh | Skeletal Mesh | `#f0ad4e` |
| Texture | Texture | `#5cb85c`（预留） |
| Material | Material | `#9b59b6`（预留） |
| 其它 | Type 原字符串 | `#808080` |

### 决策 6：交互 — MIME 拖拽 + 视口 Drop

- **选择**：Grid 启动 `QDrag`，MIME `application/x-universeengine-softobjectpath`；`RenderViewportWidget`/`ViewportHostWidget` 接收 drop → `GameApp::LoadModelToActiveLevel(SoftPath, IdentityTransform)` → `ULevel::SpawnModelFromSoftPath`
- **限制**：仅 StaticMesh/SkeletalMesh 可 drop；其它类型弹窗提示
- **原因**：复用现有 Spawn 流程，无需新 Level API
- **备选**：双击加载 — 用户明确排除

**右键菜单：**

- 在资源管理器中显示 → `QDesktopServices::openUrl` 打开 uasset 所在目录
- Reimport → `UMeshImportFactory::Reimport`（需 SourceFile + mesh 类型）
- 复制 SoftObjectPath → 剪贴板

### 决策 7：MainWindow 集成与刷新

- **选择**：`BuildAssetBrowserPanel()` 创建底部 Dock；Import 成功、`ScanContentDirectory` 后调用 `AssetBrowserPanelWidget::RefreshFromRegistry()`
- **模式**：参照 `WorldContentPanelWidget` 的 `m_last_*_revision_` 脏检测

## 风险 / 权衡

| 风险 | 缓解 |
|------|------|
| 大 mesh CPU 缩略图 Load 慢 | 仅可见 Tile 请求；异步队列；缓存 guid+mtime |
| 扫描大量 uasset 时 header 解析耗时 | `LoadHeader` 仅读 header 段，不 Load payload；扫描在启动一次性完成 |
| 拖拽到视口与相机操作冲突 | Drop 仅在 drag enter 有合法 MIME 时 accept；默认 IdentityTransform 放置 |
| CPU 缩略图质量低于 UE 真 3D | Phase 2 DX12 Provider；接口已预留 |
| Registry Type 为空的历史 uasset | `RegisterFromDisk` fallback 到路径推导；Type 空时显示「Unknown」+ 默认图 |

## 迁移计划

1. **Phase 1**：修复 Registry 扫描 → 缩略图 Service → UI 面板 → MainWindow 集成 → 手动验证
2. **Phase 2**（后续变更）：DX12 离屏 Provider、Texture uasset 缩略图
3. **回滚**：移除 Dock 与 thumbnail 模块；Registry 扫描改回不影响 Load 路径（但 Type 过滤仍受影响，不建议回滚扫描修复）

## 待决问题

- 缩略图是否持久化到磁盘（如 `Saved/Thumbnails/`）— 建议 Phase 1 仅内存缓存，降低复杂度
- Drop 放置位置 — 本期 IdentityTransform；后续可接射线检测地面

## 模块边界

```
MainWindow
  └─ AssetBrowserPanelWidget
        ├─ AssetFolderTree (from FAssetRegistry)
        ├─ AssetListModel
        ├─ AssetTileDelegate
        └─ FAssetThumbnailService
              ├─ MeshThumbnailProvider (CPU)
              └─ DefaultThumbnailProvider

FAssetRegistry (fix scan + GetRevision)
RenderViewportWidget (drop target)
GameApp::LoadModelToActiveLevel (drop handler)
```
