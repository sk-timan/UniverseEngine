# UE 编辑器资产操作目录（引擎扫描清单）

新增资产类型时，Explore 阶段必须逐项扫描本目录，并在 `operation-matrix.md` 中为每个操作填写 **Enabled** 与 **Implement**。

## 扫描入口（代码锚点）

| 区域 | 主要文件 |
|------|----------|
| 能力/多选规则 | `src/ui/AssetBrowserItemInteraction.h/.cpp` |
| Content Browser 主面板 | `src/ui/AssetBrowserPanelWidget.h/.cpp` |
| 类型过滤 | `src/ui/AssetBrowserTypeFilterWidget.cpp` |
| 缩略图 | `src/asset/thumbnail/*`, `AssetThumbnailService.cpp` |
| 资产类型元数据 | `src/asset/AssetTypeInfo.cpp` |
| Import/Reimport | `src/asset/*ImportFactory*` |
| 删除/重命名/复制 | `src/asset/AssetDeleteService`, `AssetRenameService`, `AssetDuplicateService` |
| 拖放 MIME | `AssetBrowserPanelWidget.h`（`kSoftObjectPathMimeType`, `kFolderPathMimeType`） |
| 关卡/视口放置 | `src/ui/MainWindow.cpp`, `src/app/GameApp.cpp` |

## 选择上下文（SelectionContext）

Explore 时为每个操作标注适用上下文（可多选）：

- `SingleAsset` — 单选单个资产
- `MultiHomogeneous` — 多选同类型资产
- `MultiMixedType` — 多选不同类型资产（如 StaticMesh + Texture2D）
- `MixedFolderAsset` — 文件夹 + 资产混合多选
- `FolderSingle` / `FolderMulti` — 仅文件夹
- `GridBackground` — 网格空白处
- `ExternalFile` — 外部文件拖入（非 Registry 项）
- `N/A` — 与选择无关

## 操作分类

### A. 键盘快捷键（Keyboard）

| OpId | 操作 | 默认键 | 能力字段 |
|------|------|--------|----------|
| KB-RENAME | 重命名 | F2 | bCanRename |
| KB-COPY | 复制到剪贴板 | Ctrl+C | bCanCopy |
| KB-PASTE | 粘贴副本 | Ctrl+V | （paste 按钮状态） |
| KB-DELETE | 删除 | Delete | bCanDelete |

### B. 右键菜单（ContextMenu）

| OpId | 操作 | 能力字段 |
|------|------|----------|
| CM-SHOW-EXPLORER | 在资源管理器中显示 | bCanShowInExplorer |
| CM-COPY-SOFTPATH | 复制 SoftObjectPath | bCanCopySoftObjectPath |
| CM-DUPLICATE | 创建副本 | bCanDuplicate |
| CM-RENAME | 重命名 | bCanRename |
| CM-REIMPORT | Reimport | bCanReimport |
| CM-DELETE | 删除 | bCanDelete |
| CM-IMPORT | 导入（背景菜单） | CanImportIntoSelectedFolder |
| CM-NEW-FOLDER | 新建文件夹 | bCanCreateSubfolder |

### C. 拖放（DragDrop）

| OpId | 方向 | 说明 |
|------|------|------|
| DD-DRAG-OUT | 源 | 从网格拖出 SoftObjectPath / FolderPath |
| DD-DROP-MOVE-ASSET | 目标 | 资产/文件夹拖入文件夹（移动） |
| DD-DROP-EXTERNAL-IMPORT | 目标 | 外部文件导入到当前文件夹 |
| DD-DROP-VIEWPORT | 目标 | 拖入视口/关卡（若存在） |

### D. 点击（Click）

| OpId | 操作 |
|------|------|
| CL-DBLCLICK-FOLDER | 双击文件夹进入 |
| CL-DBLCLICK-ASSET | 双击资产（编辑器/预览/无） |
| CL-SINGLE-SELECT | 单选 |
| CL-MULTI-SELECT | Ctrl/Shift 多选 |

### E. 导入与磁盘（Import）

| OpId | 操作 |
|------|------|
| IM-MENU-IMPORT | 菜单/按钮导入 |
| IM-DRAG-IMPORT | 拖入导入 |
| IM-REIMPORT | Reimport 源文件 |
| IM-SUPPORTED-EXT | 支持的扩展名 |

### F. 展示与过滤（Display）

| OpId | 操作 |
|------|------|
| DP-THUMBNAIL | 缩略图 Provider |
| DP-TYPE-FILTER | 类型过滤器可见性 |
| DP-TOOLTIP | 悬停 Tooltip |
| DP-SEARCH | 搜索命中 |

### G. 多选/混合规则（SelectionRules）

| OpId | 规则 |
|------|------|
| SR-MULTI-DISABLE-RENAME | 多选禁用重命名 |
| SR-MIXED-DISABLE-COPY | 混合类型禁用复制/副本/Reimport |
| SR-CAP-INTERSECT | 多选能力取交集 |

## Implement 字段约定

- 有实现：写**用户可编辑**的行为描述（1–3 句，含触发条件与结果）
- 无实现 / 不适用：写 **`无`**
- 继承默认（与其他资产相同）：写 **`默认（见 StaticMesh）`** 并附代码锚点

## Enabled 字段约定

- `[x]` 对该资产类型**应启用**且**已有**或**计划在本变更实现**
- `[ ]` 对该类型**不应启用**或**尚未实现**
- `[~]` 部分实现 / 仅特定选择上下文下有效（在 Notes 说明）
