# 编辑器资产操作对照表 — {{ASSET_TYPE}}

> 变更：`editor-asset-{{ASSET_TYPE_KEBAB}}`  
> 生成：`/UE-Editor-AssetExplore {{ASSET_TYPE}}`  
> 资产 Registry Type：`{{REGISTRY_TYPE}}`  
> 扫描日期：{{DATE}}

## 摘要

| 指标 | 数量 |
|------|------|
| 操作总数 | {{TOTAL}} |
| 已启用 [x] | {{ENABLED_COUNT}} |
| 未启用 [ ] | {{DISABLED_COUNT}} |
| 部分 [~] | {{PARTIAL_COUNT}} |

## 总览

> 先在本表确认 **OpId / 操作 / 选择上下文 / 启用**；实现细节见下一节「实现详情」。  
> **不要**使用 8 列宽表；按分类拆成 4 列窄表。

### A. 键盘快捷键

| OpId | 操作 | 选择上下文 | 启用 |
|------|------|------------|------|
| KB-RENAME | 重命名 | SingleAsset | [ ] |
| KB-COPY | 复制 | SingleAsset, MultiHomogeneous | [ ] |
| KB-PASTE | 粘贴 | GridBackground | [ ] |
| KB-DELETE | 删除 | SingleAsset, MultiHomogeneous, FolderSingle | [ ] |

### B. 右键菜单

| OpId | 操作 | 选择上下文 | 启用 |
|------|------|------------|------|
| CM-SHOW-EXPLORER | 在资源管理器中显示 | SingleAsset | [ ] |
| CM-COPY-SOFTPATH | 复制 SoftObjectPath | SingleAsset | [ ] |
| CM-DUPLICATE | 创建副本 | SingleAsset, MultiHomogeneous | [ ] |
| CM-RENAME | 重命名 | SingleAsset | [ ] |
| CM-REIMPORT | Reimport | SingleAsset, MultiHomogeneous | [ ] |
| CM-DELETE | 删除 | SingleAsset, MultiHomogeneous | [ ] |
| CM-IMPORT | 导入 | GridBackground | [ ] |
| CM-NEW-FOLDER | 新建文件夹 | GridBackground | [ ] |

### C. 拖放

| OpId | 操作 | 选择上下文 | 启用 |
|------|------|------------|------|
| DD-DRAG-OUT | 拖出资产 | SingleAsset, MultiHomogeneous | [ ] |
| DD-DROP-MOVE-ASSET | 拖入文件夹移动 | N/A | [ ] |
| DD-DROP-EXTERNAL-IMPORT | 外部文件导入 | ExternalFile | [ ] |
| DD-DROP-VIEWPORT | 拖入视口 | SingleAsset | [ ] |

### D. 点击

| OpId | 操作 | 选择上下文 | 启用 |
|------|------|------------|------|
| CL-DBLCLICK-FOLDER | 双击文件夹 | FolderSingle | [ ] |
| CL-DBLCLICK-ASSET | 双击资产 | SingleAsset | [ ] |
| CL-SINGLE-SELECT | 单选 | SingleAsset | [ ] |
| CL-MULTI-SELECT | 多选 | MultiHomogeneous, MultiMixedType | [ ] |

### E. 导入

| OpId | 操作 | 选择上下文 | 启用 |
|------|------|------------|------|
| IM-MENU-IMPORT | 菜单导入 | GridBackground | [ ] |
| IM-DRAG-IMPORT | 拖入导入 | ExternalFile | [ ] |
| IM-REIMPORT | Reimport | SingleAsset, MultiHomogeneous | [ ] |
| IM-SUPPORTED-EXT | 支持扩展名 | ExternalFile | [ ] |

### F. 展示与过滤

| OpId | 操作 | 选择上下文 | 启用 |
|------|------|------------|------|
| DP-THUMBNAIL | 缩略图 | N/A | [ ] |
| DP-TYPE-FILTER | 类型过滤 | N/A | [ ] |
| DP-TOOLTIP | 悬停 Tooltip | SingleAsset | [ ] |
| DP-SEARCH | 搜索 | N/A | [ ] |

### G. 多选 / 混合规则

| OpId | 操作 | 选择上下文 | 启用 |
|------|------|------------|------|
| SR-MULTI-DISABLE-RENAME | 多选禁重命名 | MultiHomogeneous, MultiMixedType | [ ] |
| SR-MIXED-DISABLE-COPY | 混合禁复制/副本/Reimport | MultiMixedType, MixedFolderAsset | [ ] |
| SR-CAP-INTERSECT | 多选能力交集 | MultiHomogeneous | [ ] |

---

## 实现详情

> **Implement** 列可自由修改；Apply 阶段以本节为准。  
> 每个 OpId 使用 **字段 | 内容** 两列表，与总览 OpId 一一对应。

### {{OPID}} · {{OPERATION_NAME}}

| 字段 | 内容 |
|------|------|
| 分类 | {{CATEGORY}} |
| 选择上下文 | {{SELECTION_CONTEXT}} |
| 启用 | [ ] |
| Implement | 无 |
| CodeAnchor | |
| Notes | |

（为 catalog 中每个 OpId 复制上述块并填写）

---

## 待定制扩展点

- [ ] `AssetTypeInfo` 显示名 / 可渲染 / Reimport 判定
- [ ] `GetAssetItemCapabilities` 按类型覆盖
- [ ] `AssetBrowserTypeFilterWidget` 独立筛选项
- [ ] 视口/关卡 Drop Handler
- [ ] 双击资产行为

## 用户备注

（Explore 完成后可在此补充产品需求；Apply 前请确认 Implement 已编辑完毕。）
