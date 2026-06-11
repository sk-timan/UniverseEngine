# editor-asset-texture2d — 实现任务

> 变更：`editor-asset-texture2d`  
> 生成：`/UE-Editor-AssetApply editor-asset-texture2d`  
> 规格来源：`operation-matrix.md`

## 1. Capability 与选择规则

- [x] 1.1 确认 `GetAssetItemCapabilities`：Texture2D 启用 Reimport（与 Mesh 共用默认 + 类型判断）
- [x] 1.2 确认 `ApplySelectionLevelCapabilityRules`：多选禁重命名；混合多选仍允许 Copy/Duplicate/Reimport
- [x] 1.3 确认 `IntersectItemCapabilities`：同类型多选 Texture 能力取交集

## 2. 快捷键与菜单

- [x] 2.1 KB-RENAME / CM-RENAME：F2 与右键重命名（已有）
- [x] 2.2 KB-COPY / KB-PASTE / KB-DELETE：复制粘贴删除（已有）
- [x] 2.3 CM-SHOW-EXPLORER / CM-COPY-SOFTPATH / CM-DUPLICATE / CM-DELETE（已有）
- [x] 2.4 CM-REIMPORT：修复右键单选 Reimport 仅走 Mesh 工厂的问题，统一调用 `ReimportSelectedAssets`
- [x] 2.5 CM-IMPORT / CM-NEW-FOLDER（已有）
- [x] 2.6 修复右键菜单：exec 后选中丢失，快照并恢复 `selectedIndexes`

## 3. 拖放与导入

- [x] 3.1 DD-DRAG-OUT / DD-DROP-MOVE-ASSET / DD-DROP-EXTERNAL-IMPORT（已有）
- [x] 3.2 IM-MENU-IMPORT / IM-DRAG-IMPORT / IM-REIMPORT / IM-SUPPORTED-EXT（已有）
- [ ] 3.3 DD-DROP-VIEWPORT：跳过（Enabled=[ ]，用户未勾选扩展点）

## 4. 展示与交互

- [x] 4.1 DP-THUMBNAIL：`TextureThumbnailProvider`（已有）
- [x] 4.2 DP-TYPE-FILTER：`AssetBrowserTypeFilterWidget` 增加 Texture 2D 独立筛选项
- [x] 4.3 DP-TOOLTIP / DP-SEARCH（已有默认行为）
- [x] 4.4 CL-DBLCLICK-ASSET：双击 Texture2D 打开 `TexturePreviewDialog` 预览
- [x] 4.5 CL-DBLCLICK-FOLDER / CL-SINGLE-SELECT / CL-MULTI-SELECT（已有）

## 5. 测试与文档

- [x] 5.1 生成 `test-matrix.md` 手动验收步骤
- [x] 5.2 同步 `operation-matrix.md` Implement / Enabled
- [x] 5.3 更新 `.ue-editor-asset.yaml` → `apply_status: done`
