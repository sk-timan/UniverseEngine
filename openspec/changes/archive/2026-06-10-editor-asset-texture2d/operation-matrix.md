# 编辑器资产操作对照表 — Texture2D

> 变更：`editor-asset-texture2d`  
> 生成：`/UE-Editor-AssetExplore 新建的Texture资产`  
> 资产 Registry Type：`Texture2D`  
> 扫描日期：2026-06-10

## 摘要

| 指标 | 数量 |
|------|------|
| 操作总数 | 29 |
| 已启用 [x] | 26 |
| 未启用 [ ] | 3 |
| 部分 [~] | 0 |

## 总览

> 先在本表确认 **OpId / 操作 / 选择上下文 / 启用**；实现细节见下一节「实现详情」。

### A. 键盘快捷键

| OpId | 操作 | 选择上下文 | 启用 |
|------|------|------------|------|
| KB-RENAME | 重命名 | SingleAsset | [x] |
| KB-COPY | 复制 | SingleAsset, MultiHomogeneous | [x] |
| KB-PASTE | 粘贴 | GridBackground | [x] |
| KB-DELETE | 删除 | SingleAsset, MultiHomogeneous, FolderSingle | [x] |

### B. 右键菜单

| OpId | 操作 | 选择上下文 | 启用 |
|------|------|------------|------|
| CM-SHOW-EXPLORER | 在资源管理器中显示 | SingleAsset | [x] |
| CM-COPY-SOFTPATH | 复制 SoftObjectPath | SingleAsset | [x] |
| CM-DUPLICATE | 创建副本 | SingleAsset, MultiHomogeneous | [x] |
| CM-RENAME | 重命名 | SingleAsset | [x] |
| CM-REIMPORT | Reimport | SingleAsset, MultiHomogeneous | [x] |
| CM-DELETE | 删除 | SingleAsset, MultiHomogeneous | [x] |
| CM-IMPORT | 导入 | GridBackground | [x] |
| CM-NEW-FOLDER | 新建文件夹 | GridBackground | [x] |

### C. 拖放

| OpId | 操作 | 选择上下文 | 启用 |
|------|------|------------|------|
| DD-DRAG-OUT | 拖出资产 | SingleAsset, MultiHomogeneous | [x] |
| DD-DROP-MOVE-ASSET | 拖入文件夹移动 | N/A | [x] |
| DD-DROP-EXTERNAL-IMPORT | 外部文件导入 | ExternalFile | [x] |
| DD-DROP-VIEWPORT | 拖入视口 | SingleAsset | [ ] |

### D. 点击

| OpId | 操作 | 选择上下文 | 启用 |
|------|------|------------|------|
| CL-DBLCLICK-FOLDER | 双击文件夹 | FolderSingle | [x] |
| CL-DBLCLICK-ASSET | 双击资产 | SingleAsset | [x] |
| CL-SINGLE-SELECT | 单选 | SingleAsset | [x] |
| CL-MULTI-SELECT | 多选 | MultiHomogeneous, MultiMixedType | [x] |

### E. 导入

| OpId | 操作 | 选择上下文 | 启用 |
|------|------|------------|------|
| IM-MENU-IMPORT | 菜单导入 | GridBackground | [x] |
| IM-DRAG-IMPORT | 拖入导入 | ExternalFile | [x] |
| IM-REIMPORT | Reimport | SingleAsset, MultiHomogeneous | [x] |
| IM-SUPPORTED-EXT | 支持扩展名 | ExternalFile | [x] |

### F. 展示与过滤

| OpId | 操作 | 选择上下文 | 启用 |
|------|------|------------|------|
| DP-THUMBNAIL | 缩略图 | N/A | [x] |
| DP-TYPE-FILTER | 类型过滤 | N/A | [x] |
| DP-TOOLTIP | 悬停 Tooltip | SingleAsset | [x] |
| DP-SEARCH | 搜索 | N/A | [x] |

### G. 多选 / 混合规则

| OpId | 操作 | 选择上下文 | 启用 |
|------|------|------------|------|
| SR-MULTI-DISABLE-RENAME | 多选禁重命名 | MultiHomogeneous, MultiMixedType | [x] |
| SR-MIXED-DISABLE-COPY | 混合多选规则 | MultiMixedType, MixedFolderAsset | [x] |
| SR-CAP-INTERSECT | 多选能力交集 | MultiHomogeneous | [x] |

---

## 实现详情

> **Implement** 列可自由修改；Apply 阶段以本节为准。

### KB-RENAME · 重命名

| 字段 | 内容 |
|------|------|
| 分类 | Keyboard |
| 选择上下文 | SingleAsset |
| 启用 | [x] |
| Implement | F2：对单选 Texture2D 进入网格内联重命名，调用 `FAssetRenameService` 更新 uasset/meta/registry。 |
| CodeAnchor | `AssetBrowserPanelWidget.cpp` · `BeginRenameSelectedGridItem` |
| Notes | 多选时禁用（SR-MULTI） |

### KB-COPY · 复制

| 字段 | 内容 |
|------|------|
| 分类 | Keyboard |
| 选择上下文 | SingleAsset, MultiHomogeneous |
| 启用 | [x] |
| Implement | Ctrl+C：将选中 Texture 的 `FAssetRegistryEntry` 存入 `m_copied_asset_entries_`，供 Paste 使用。 |
| CodeAnchor | `CopySelectedAsset` |
| Notes | 混合类型多选禁用 |

### KB-PASTE · 粘贴

| 字段 | 内容 |
|------|------|
| 分类 | Keyboard |
| 选择上下文 | GridBackground |
| 启用 | [x] |
| Implement | Ctrl+V：在当前选中 Content 文件夹下对 copied entries 创建副本（`FAssetDuplicateService`）。 |
| CodeAnchor | `PasteCopiedAsset` |
| Notes | 需目标文件夹可导入 |

### KB-DELETE · 删除

| 字段 | 内容 |
|------|------|
| 分类 | Keyboard |
| 选择上下文 | SingleAsset, MultiHomogeneous, FolderSingle |
| 启用 | [x] |
| Implement | Delete：删除选中 Texture uasset/meta 与 registry 项；文件夹树聚焦时删文件夹。 |
| CodeAnchor | `DeleteSelectedGridItems` · `TryHandleDeleteShortcut` |
| Notes | 无 |

### CM-SHOW-EXPLORER · 在资源管理器中显示

| 字段 | 内容 |
|------|------|
| 分类 | ContextMenu |
| 选择上下文 | SingleAsset |
| 启用 | [x] |
| Implement | 打开 uasset 所在磁盘目录（资源管理器）。 |
| CodeAnchor | `OnGridContextMenuRequested` |
| Notes | 多选禁用 |

### CM-COPY-SOFTPATH · 复制 SoftObjectPath

| 字段 | 内容 |
|------|------|
| 分类 | ContextMenu |
| 选择上下文 | SingleAsset |
| 启用 | [x] |
| Implement | 复制 `AssetPath.ObjectName` 到系统剪贴板。 |
| CodeAnchor | `OnGridContextMenuRequested` |
| Notes | 无 |

### CM-DUPLICATE · 创建副本

| 字段 | 内容 |
|------|------|
| 分类 | ContextMenu |
| 选择上下文 | SingleAsset, MultiHomogeneous |
| 启用 | [x] |
| Implement | 在当前左侧选中文件夹下复制 uasset，生成新 ObjectName。 |
| CodeAnchor | `DuplicateSelectedAssets` |
| Notes | 混合类型禁用 |

### CM-RENAME · 重命名

| 字段 | 内容 |
|------|------|
| 分类 | ContextMenu |
| 选择上下文 | SingleAsset |
| 启用 | [x] |
| Implement | 同 KB-RENAME。 |
| CodeAnchor | `BeginRenameSelectedGridItem` |
| Notes | 无 |

### CM-REIMPORT · Reimport

| 字段 | 内容 |
|------|------|
| 分类 | ContextMenu |
| 选择上下文 | SingleAsset, MultiHomogeneous |
| 启用 | [x] |
| Implement | 读取 Entry.SourceFile，调用 `UTextureImportFactory::Reimport`；保留 guid/asset_path，刷新 payload 与缩略图缓存。 |
| CodeAnchor | `ReimportSelectedAssets` |
| Notes | 需 SourceFile 非空 |

### CM-DELETE · 删除

| 字段 | 内容 |
|------|------|
| 分类 | ContextMenu |
| 选择上下文 | SingleAsset, MultiHomogeneous |
| 启用 | [x] |
| Implement | 同 KB-DELETE。 |
| CodeAnchor | `DeleteSelectedAssets` |
| Notes | 无 |

### CM-IMPORT · 导入

| 字段 | 内容 |
|------|------|
| 分类 | ContextMenu |
| 选择上下文 | GridBackground |
| 启用 | [x] |
| Implement | 打开文件对话框；图片走 `ImportTextureDialog` + `GameApp::ImportTextureFromSourceFile`，模型走原有 Mesh 流程。 |
| CodeAnchor | `OnImportAssetsRequested` |
| Notes | 无 |

### CM-NEW-FOLDER · 新建文件夹

| 字段 | 内容 |
|------|------|
| 分类 | ContextMenu |
| 选择上下文 | GridBackground |
| 启用 | [x] |
| Implement | 在当前 Content 子目录创建文件夹。 |
| CodeAnchor | `OnAddNewFolderRequested` |
| Notes | 无 |

### DD-DRAG-OUT · 拖出资产

| 字段 | 内容 |
|------|------|
| 分类 | DragDrop |
| 选择上下文 | SingleAsset, MultiHomogeneous |
| 启用 | [x] |
| Implement | 拖放 MIME `application/x-universeengine-softobjectpath`；供拖入其他文件夹或（若支持）视口。 |
| CodeAnchor | `AssetBrowserListView::startDrag` |
| Notes | Texture 无专属 payload |

### DD-DROP-MOVE-ASSET · 拖入文件夹移动

| 字段 | 内容 |
|------|------|
| 分类 | DragDrop |
| 选择上下文 | N/A |
| 启用 | [x] |
| Implement | 将 SoftObjectPath / FolderPath 拖入目标文件夹，更新磁盘路径与 registry。 |
| CodeAnchor | `OnItemsDroppedToFolder` |
| Notes | 与类型无关 |

### DD-DROP-EXTERNAL-IMPORT · 外部文件导入

| 字段 | 内容 |
|------|------|
| 分类 | DragDrop |
| 选择上下文 | ExternalFile |
| 启用 | [x] |
| Implement | 拖入 png/jpg/jpeg/tga/bmp → 默认 ImportSettings 导入 Texture2D；模型扩展名走 Mesh Import。 |
| CodeAnchor | `OnExternalFilesDropped` |
| Notes | 菜单导入可弹对话框，拖入用默认参数 |

### DD-DROP-VIEWPORT · 拖入视口

| 字段 | 内容 |
|------|------|
| 分类 | DragDrop |
| 选择上下文 | SingleAsset |
| 启用 | [ ] |
| Implement | 无 |
| CodeAnchor | — |
| Notes | 引擎暂无 Texture 拖入视口/关卡逻辑 |

### CL-DBLCLICK-FOLDER · 双击文件夹

| 字段 | 内容 |
|------|------|
| 分类 | Click |
| 选择上下文 | FolderSingle |
| 启用 | [x] |
| Implement | 双击文件夹 tile 同步左侧树选中路径。 |
| CodeAnchor | `OnAssetGridDoubleClicked` |
| Notes | 无 |

### CL-DBLCLICK-ASSET · 双击资产

| 字段 | 内容 |
|------|------|
| 分类 | Click |
| 选择上下文 | SingleAsset |
| 启用 | [x] |
| Implement | 双击 Texture2D tile 打开 `TexturePreviewDialog`，显示 mip0 预览与 SoftObjectPath/尺寸/SourceFile。 |
| CodeAnchor | `AssetBrowserPanelWidget.cpp` · `OnAssetGridDoubleClicked` · `TexturePreviewDialog` |
| Notes | Mesh 仍无双击行为 |

### CL-SINGLE-SELECT · 单选

| 字段 | 内容 |
|------|------|
| 分类 | Click |
| 选择上下文 | SingleAsset |
| 启用 | [x] |
| Implement | 选中高亮，更新状态栏与可用快捷键/菜单能力。 |
| CodeAnchor | `OnGridSelectionChanged` |
| Notes | 无 |

### CL-MULTI-SELECT · 多选

| 字段 | 内容 |
|------|------|
| 分类 | Click |
| 选择上下文 | MultiHomogeneous, MultiMixedType |
| 启用 | [x] |
| Implement | Ctrl/Shift 多选；能力取交集并应用 SR-* 规则。 |
| CodeAnchor | `BuildGridSelectionState` |
| Notes | 无 |

### IM-MENU-IMPORT · 菜单导入

| 字段 | 内容 |
|------|------|
| 分类 | Import |
| 选择上下文 | GridBackground |
| 启用 | [x] |
| Implement | 同 CM-IMPORT；图片文件弹出 `ImportTextureDialog` 设置 sRGB/max_size/flip_y。 |
| CodeAnchor | `OnImportAssetsRequested` |
| Notes | 无 |

### IM-DRAG-IMPORT · 拖入导入

| 字段 | 内容 |
|------|------|
| 分类 | Import |
| 选择上下文 | ExternalFile |
| 启用 | [x] |
| Implement | 同 DD-DROP-EXTERNAL-IMPORT。 |
| CodeAnchor | `OnExternalFilesDropped` |
| Notes | 无 |

### IM-REIMPORT · Reimport

| 字段 | 内容 |
|------|------|
| 分类 | Import |
| 选择上下文 | SingleAsset, MultiHomogeneous |
| 启用 | [x] |
| Implement | 同 CM-REIMPORT。 |
| CodeAnchor | `UTextureImportFactory::Reimport` |
| Notes | 无 |

### IM-SUPPORTED-EXT · 支持扩展名

| 字段 | 内容 |
|------|------|
| 分类 | Import |
| 选择上下文 | ExternalFile |
| 启用 | [x] |
| Implement | png, jpg, jpeg, tga, bmp（`IsImportableImageExtension`）；EXR 在解码层拒绝。 |
| CodeAnchor | `ImageDecoder.cpp` |
| Notes | 无 |

### DP-THUMBNAIL · 缩略图

| 字段 | 内容 |
|------|------|
| 分类 | Display |
| 选择上下文 | N/A |
| 启用 | [x] |
| Implement | `TextureThumbnailProvider` 读 uasset mip0 像素生成 QImage；`FAssetThumbnailService` 异步缓存。 |
| CodeAnchor | `TextureThumbnailProvider.cpp` |
| Notes | 无 |

### DP-TYPE-FILTER · 类型过滤

| 字段 | 内容 |
|------|------|
| 分类 | Display |
| 选择上下文 | N/A |
| 启用 | [x] |
| Implement | `AssetBrowserTypeFilterWidget` 菜单含 Texture 2D 勾选项；「其它」不再包含 Texture2D/Texture。 |
| CodeAnchor | `AssetBrowserTypeFilterWidget.cpp` |
| Notes | 与 StaticMesh/SkeletalMesh 并列 |

### DP-TOOLTIP · 悬停 Tooltip

| 字段 | 内容 |
|------|------|
| 分类 | Display |
| 选择上下文 | SingleAsset |
| 启用 | [x] |
| Implement | 默认（与 StaticMesh 相同）：显示 SoftObjectPath、类型等。 |
| CodeAnchor | `AssetBrowserItemTooltipWidget` |
| Notes | 无 |

### DP-SEARCH · 搜索

| 字段 | 内容 |
|------|------|
| 分类 | Display |
| 选择上下文 | N/A |
| 启用 | [x] |
| Implement | 默认：按 ObjectName / AssetPath 文本过滤；搜索时自动切「全部类型」。 |
| CodeAnchor | `ApplyFilters` |
| Notes | 无 |

### SR-MULTI-DISABLE-RENAME · 多选禁重命名

| 字段 | 内容 |
|------|------|
| 分类 | SelectionRules |
| 选择上下文 | MultiHomogeneous, MultiMixedType |
| 启用 | [x] |
| Implement | 多选时 `bCanRename=false`（含 Texture 多选）。 |
| CodeAnchor | `ApplySelectionLevelCapabilityRules` |
| Notes | 无 |

### SR-MIXED-DISABLE-COPY · 混合禁复制/副本/Reimport

| 字段 | 内容 |
|------|------|
| 分类 | SelectionRules |
| 选择上下文 | MultiMixedType, MixedFolderAsset |
| 启用 | [x] |
| Implement | 混合多选（含 Texture+StaticMesh、文件夹+资产）仍允许 Copy/Duplicate/Reimport；仅禁用 ShowInExplorer / CopySoftObjectPath。 |
| CodeAnchor | `ApplySelectionLevelCapabilityRules` |
| Notes | `bIsMixedAssetTypeSelection` 标记异类型资产多选 |

### SR-CAP-INTERSECT · 多选能力交集

| 字段 | 内容 |
|------|------|
| 分类 | SelectionRules |
| 选择上下文 | MultiHomogeneous |
| 启用 | [x] |
| Implement | 多个 Texture2D 同时选中时能力为各条目交集（Reimport 等均 true）。 |
| CodeAnchor | `IntersectItemCapabilities` |
| Notes | 无 |

---

## 待定制扩展点

- [ ] `GetAssetItemCapabilities` 按 `Texture2D` 独立函数（当前与全资产共用默认 + Reimport 类型判断）
- [x] `AssetBrowserTypeFilterWidget` 增加 Texture2D 独立筛选项
- [x] `OnAssetGridDoubleClicked`：Texture 双击预览或属性面板
- [ ] 视口/关卡 DragDrop：拖 Texture 到 RenderViewport 的行为
- [x] 混合多选 Texture+StaticMesh 时允许批量 Duplicate/Reimport

## 用户备注

Texture2D 核心 Import/Load/Reimport/缩略图已实现；Explore 阶段标记的 `[ ]` 项为**产品可选增强**，Apply 时按需勾选启用。
