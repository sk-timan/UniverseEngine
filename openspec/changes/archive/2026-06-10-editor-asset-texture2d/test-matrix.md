# 编辑器资产操作测试对照表 — Texture2D

> 变更：`editor-asset-texture2d`  
> 生成：`/UE-Editor-AssetApply editor-asset-texture2d`  
> 对应：`operation-matrix.md`  
> 验收日期：2026-06-10

## 测试摘要

| 指标 | 数量 |
|------|------|
| 需测项（Enabled=[x]） | 26 |
| 已通过 | 26 |
| 未测 / 失败 | 0 |

## 手动测试

| OpId | 操作 | 前置条件 | 步骤 | 期望结果 | 状态 |
|------|------|----------|------|----------|------|
| KB-RENAME | 重命名 | Content 下已有 Texture2D，单选 | 选中资产按 F2，修改名称回车 | uasset/meta/registry 更新，网格显示新名 | [x] |
| KB-COPY | 复制 | 单选或多选同类型 Texture | Ctrl+C | 可复制 | [x] |
| KB-PASTE | 粘贴 | 已 Copy Texture，选中目标文件夹 | Ctrl+V | 当前文件夹下生成副本 | [x] |
| KB-DELETE | 删除 | 单选或多选 Texture / 文件夹 | Delete | 磁盘与 registry 删除成功 | [x] |
| CM-SHOW-EXPLORER | 资源管理器 | 单选 Texture | 右键 → 在资源管理器中显示 | 打开并选中 uasset 文件 | [x] |
| CM-COPY-SOFTPATH | 复制路径 | 单选 Texture | 右键 → 复制 SoftObjectPath | 剪贴板为 `Path.ObjectName` | [x] |
| CM-DUPLICATE | 创建副本 | 单选或多选 | 右键 → 创建副本 | 同目录生成副本 | [x] |
| CM-RENAME | 重命名 | 单选 Texture | 右键 → 重命名 | 同 KB-RENAME | [x] |
| CM-REIMPORT | Reimport | Texture 有 SourceFile | 右键 → Reimport | 刷新 payload 与缩略图 | [x] |
| CM-DELETE | 删除 | 选中 Texture | 右键 → 删除 | 同 KB-DELETE | [x] |
| CM-IMPORT | 导入 | 选中 Content 子文件夹 | 空白处右键 → 导入，选 png | ImportTextureDialog 或导入成功 | [x] |
| CM-NEW-FOLDER | 新建文件夹 | 选中 Content 子文件夹 | 空白处右键 → 新建文件夹 | 创建新文件夹 | [x] |
| DD-DRAG-OUT | 拖出资产 | 单选 Texture | 拖出到另一文件夹 | MIME 含 SoftObjectPath，移动成功 | [x] |
| DD-DROP-MOVE-ASSET | 文件夹移动 | 两文件夹存在 | 拖 Texture 到目标文件夹 tile | registry 路径更新 | [x] |
| DD-DROP-EXTERNAL-IMPORT | 外部导入 | Content 文件夹选中 | 拖入 png/jpg 到网格 | 默认设置导入 Texture2D | [x] |
| CL-DBLCLICK-FOLDER | 双击文件夹 | 文件夹 tile 可见 | 双击文件夹 | 左侧树同步选中 | [x] |
| CL-DBLCLICK-ASSET | 双击资产 | Texture2D 已导入 | 双击 Texture tile | 打开纹理预览对话框 | [x] |
| CL-SINGLE-SELECT | 单选 | 网格有 Texture | 单击选中 | 高亮，菜单/快捷键可用 | [x] |
| CL-MULTI-SELECT | 多选 | 多个 Texture | Ctrl/Shift 多选 | 状态栏显示选中数，能力取交集 | [x] |
| IM-MENU-IMPORT | 菜单导入 | 同 CM-IMPORT | 导入 png 并配置 sRGB/max_size | 写入 Content，网格刷新 | [x] |
| IM-DRAG-IMPORT | 拖入导入 | 同 DD-DROP-EXTERNAL-IMPORT | 拖入图片 | 默认参数导入 | [x] |
| IM-REIMPORT | Reimport | 同 CM-REIMPORT | 多选同类型 Texture Reimport | 全部成功或部分失败有提示 | [x] |
| IM-SUPPORTED-EXT | 支持扩展名 | 外部 png/jpg/tga/bmp | 拖入或导入 | 接受；exr 拒绝 | [x] |
| DP-THUMBNAIL | 缩略图 | Texture 已 Cook | 滚动网格 | 显示 mip0 缩略图 | [x] |
| DP-TYPE-FILTER | 类型过滤 | 目录含 Mesh 与 Texture | 类型过滤选 Texture 2D | 仅显示 Texture2D/Texture | [x] |
| DP-TOOLTIP | Tooltip | 单选 Texture | 悬停 tile | 显示路径与类型 | [x] |
| DP-SEARCH | 搜索 | 多个资产 | 搜索框输入 ObjectName 片段 | 过滤结果正确 | [x] |
| SR-MULTI-DISABLE-RENAME | 多选禁重命名 | 多选 Texture | F2 / 右键重命名 | 重命名不可用 | [x] |
| SR-MIXED-DISABLE-COPY | 混合多选 | Texture+StaticMesh 多选 | 右键 Duplicate/Reimport | 均可批量执行 | [x] |
| SR-CAP-INTERSECT | 能力交集 | 多选同类型 Texture | 右键 | Reimport 等仍可用 | [x] |

## 自动化测试

| TestId | 覆盖 OpId | 测试文件 | 状态 |
|--------|-----------|----------|------|
| TextureSerializerTests | IM-REIMPORT / DP-THUMBNAIL 数据层 | `OpenSpecTest_TextureSerializerTests` | [x] |

## 回归

| 范围 | 状态 |
|------|------|
| StaticMesh 现有操作未破坏 | [x] |
| 混合多选规则仍符合 SR-* 行 | [x] |

## 备注

- 验收后修复：右键菜单依赖 `SelectionSnapshot` 恢复选中（见 `ACCEPTANCE.md`）。
- 未做：DD-DROP-VIEWPORT、独立 `GetTextureAssetItemCapabilities`。
