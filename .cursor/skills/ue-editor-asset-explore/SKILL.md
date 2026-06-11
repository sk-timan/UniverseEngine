---
name: ue-editor-asset-explore
description: >-
  扫描 Content Browser / 编辑器中与指定资产类型相关的全部交互操作，生成可勾选对照表
  （Enabled + 可编辑 Implement）。在用户输入 /UE-Editor-AssetExplore、
  UE-Editor-AssetExplore、或「新建 XX 资产的编辑器操作对照表」时使用。
disable-model-invocation: true
license: MIT
metadata:
  author: OpenSpecTest
  version: "1.0"
---

# UE Editor Asset Explore

**命令**：`/UE-Editor-AssetExplore <资产描述>`

示例：`/UE-Editor-AssetExplore 新建的Texture资产` → 资产类型 `Texture2D`

---

## 目标

1. 从代码库**自动收集**该资产类型相关的编辑器操作（快捷键、右键、拖放、Delete、多选/混合多选等）
2. 生成 **operation-matrix.md** 对照表：每项含 **Enabled**、**Implement**（预设行为描述，无则写「无」，均可修改）
3. **不编写实现代码**（Explore 阶段只产出文档）

---

## 输入解析

1. 从用户参数提取 **资产类型名**（如 `Texture2D`、`StaticMesh`、中文「纹理」→ `Texture2D`）
2. 推导 **kebab 变更名**：`editor-asset-<kebab-type>`（例：`editor-asset-texture2d`）
3. 推导 **Registry Type 字符串**（与 `FAssetRegistryEntry.Type` / `AssetTypeInfo` 一致）

若类型不明确，用 AskQuestion 让用户选择或确认 Registry Type。

---

## 输出位置

```
openspec/changes/editor-asset-<kebab-type>/
├── .ue-editor-asset.yaml      # 元数据（见下）
├── operation-matrix.md        # 主产出物
└── scope.md                   # 可选：扫描范围与未覆盖项
```

### `.ue-editor-asset.yaml` 模板

```yaml
asset_type: Texture2D
asset_type_kebab: texture2d
registry_type: Texture2D
explore_command: "/UE-Editor-AssetExplore 新建的Texture资产"
explored_at: "2026-06-10"
status: explored
apply_status: pending
source_catalog: ".cursor/skills/ue-editor-asset/operation-catalog.md"
```

---

## 执行步骤

### 1. 阅读规范

- [operation-catalog.md](../ue-editor-asset/operation-catalog.md) — 必扫操作清单与扫描入口
- [operation-matrix.template.md](../ue-editor-asset/templates/operation-matrix.template.md) — 表格结构

### 2. 扫描代码库

按 catalog 中的**扫描入口**搜索，对每个 OpId：

1. 定位 **CodeAnchor**（文件:符号 或 行号范围）
2. 判断对该资产类型 **Enabled** 应为 `[x]` / `[ ]` / `[~]`
3. 撰写 **Implement**：
   - 有逻辑：1–3 句（触发条件 → 调用链 → 结果）
   - 无逻辑或不适用：写 **`无`**
   - 与其他类型共用默认：写 **`默认（与 StaticMesh 相同）`** + 锚点

**必须**单独验证：

- `GetAssetItemCapabilities` / `ApplySelectionLevelCapabilityRules`
- `AssetBrowserPanelWidget` 快捷键与 `eventFilter`
- 右键菜单 `OnGridContextMenuRequested` / `ShowGridBackgroundContextMenu`
- 拖放：`HasImportableAssetFileMimeData`、`HasContentBrowserDropMimeData`、`startDrag`
- `ReimportSelectedAssets`、`DeleteSelectedGridItems`、`DuplicateSelectedAssets`
- Import 对话框与 Factory（如 `UTextureImportFactory`）
- 缩略图 Provider、`AssetTypeInfo`

### 3. 生成 operation-matrix.md

- 复制 template，替换 `{{ASSET_TYPE}}`、`{{DATE}}` 等
- **格式要求**：
  - **总览**：按 A–G 分类，每类一张 **4 列窄表**（OpId | 操作 | 选择上下文 | 启用）
  - **实现详情**：每个 OpId 一个小节，**字段 | 内容** 两列表（Implement / CodeAnchor / Notes）
  - **禁止** 8 列单行宽表（长文本会导致列对不齐、难以扫读）
- **填满每一行**（catalog 中 A–G 全部 OpId + SR-*）
- 更新摘要计数
- 填写「待定制扩展点」勾选（相对默认还缺什么）

### 4. 创建 scope.md（推荐）

记录：

- 本次扫描到的文件列表
- 未在引擎内找到的能力（如视口 Drop 若不存在，标明「引擎暂无」）
- 建议 Apply 阶段优先实现的 `[ ]` 项

### 5. 完成输出

```
## Explore 完成

**变更：** editor-asset-<kebab-type>
**资产类型：** <RegistryType>
**对照表：** openspec/changes/editor-asset-<kebab-type>/operation-matrix.md

- 操作总数：N
- 已启用 [x]：A
- 未启用 [ ]：B
- 部分 [~]：C

请审阅并修改 **Implement** 列。确认后运行：
`/UE-Editor-AssetApply editor-asset-<kebab-type>`
```

---

## 护栏

- Explore **禁止**改 C++ / UI 实现（仅写 openspec 变更目录下的 md/yaml）
- Implement 列必须**可读、可改**；不要留空
- Enabled 与代码现状不一致时，Notes 注明「代码已支持但 matrix 标记为待启用」或反之
- 混合多选、文件夹+资产混合等多上下文规则必须引用 `ApplySelectionLevelCapabilityRules`

---

## 参考示例

- [examples/texture2d-operation-matrix.md](../ue-editor-asset/examples/texture2d-operation-matrix.md) — Texture2D 完整样例
