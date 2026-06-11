---
name: ue-editor-asset-apply
description: >-
  根据 editor-asset 变更中的 operation-matrix.md 实现 Enabled 为 [x] 的编辑器操作，
  更新 Implement 与代码，并生成 test-matrix.md 测试对照表。在用户输入
  /UE-Editor-AssetApply、UE-Editor-AssetApply 或要求实现编辑器资产交互时使用。
disable-model-invocation: true
license: MIT
metadata:
  author: OpenSpecTest
  version: "1.0"
---

# UE Editor Asset Apply

**命令**：`/UE-Editor-AssetApply [变更名]`

示例：

- `/UE-Editor-AssetApply` — 使用对话上下文或唯一的 `editor-asset-*` 变更
- `/UE-Editor-AssetApply editor-asset-texture2d`

---

## 前置条件

1. 存在 `openspec/changes/<change>/operation-matrix.md`（由 Explore 生成）
2. 用户已审阅 **Implement** 列（Apply 以 matrix 为规格来源）
3. `.ue-editor-asset.yaml` 中 `status: explored` 或 `apply_status: pending`

若缺少 operation-matrix，提示先运行 `/UE-Editor-AssetExplore`。

---

## 目标

1. 读取对照表中 **Enabled = [x]** 或 **Enabled = [~]** 的条目
2. 在代码中实现或修正对应行为（类型分支、Capability、Handler）
3. 完成后生成 **test-matrix.md** 与 **tasks.md**
4. 更新 `.ue-editor-asset.yaml` → `apply_status: done`

---

## 执行步骤

### 1. 定位变更

```text
openspec/changes/editor-asset-<kebab-type>/
├── .ue-editor-asset.yaml
├── operation-matrix.md    # 读
├── test-matrix.md         # 写
└── tasks.md               # 写
```

若未指定变更名：

- 仅一个 `editor-asset-*` 目录 → 自动选择
- 否则 AskQuestion 让用户选择

### 2. 解析 operation-matrix.md

- 提取每行：OpId、Enabled、Implement、CodeAnchor、SelectionContext
- **跳过** Enabled = `[ ]` 的项（除非用户明确要求实现）
- **Implement = `无`** 且 Enabled = `[x]` → 暂停并 AskQuestion（规格矛盾）

### 3. 生成 tasks.md

按 OpId 分组创建可验证任务，格式：

```markdown
## 1. Capability 与选择规则
- [ ] 1.1 GetAssetItemCapabilities：Texture2D Reimport ...
## 2. 快捷键与菜单
...
## 3. 拖放
...
## 4. 测试
- [ ] 4.1 生成 test-matrix 手动步骤
- [ ] 4.2 补充自动化测试（若适用）
```

### 4. 实现（循环 tasks）

对每个任务：

1. 修改最小代码范围（优先 `AssetBrowserItemInteraction`、`AssetTypeInfo`、类型 Factory、Panel 分支）
2. 保持与 **Implement** 列描述一致
3. 完成后在 **tasks.md** 标记 `[x]`
4. 若实现与 Implement 不一致，**回写 operation-matrix.md** 的 Implement 列

**常见扩展点**：

| 需求 | 典型改动 |
|------|----------|
| 类型专属 Capability | `GetAssetItemCapabilities` 或 `Get<Type>AssetItemCapabilities` |
| Reimport | `*ImportFactory::Reimport` + `ReimportSelectedAssets` 分支 |
| 外部拖入 | `IsImportable*Extension` + `OnExternalFilesDropped` |
| 缩略图 | `*ThumbnailProvider` + `AssetThumbnailService` 注册 |
| 类型过滤 | `AssetBrowserTypeFilterWidget` |
| 双击资产 | `OnAssetGridDoubleClicked` 按 Type 分支 |

### 5. 生成 test-matrix.md

使用 [test-matrix.template.md](../ue-editor-asset/templates/test-matrix.template.md)：

- 每个 **Enabled=[x]** 的 OpId 至少一行手动测试（前置、步骤、期望）
- 标注可自动化项（如 Capability 单元测试、Import 测试）
- 包含 **StaticMesh 回归** 行

### 6. 验证

- 编译 `OpenSpecTest`
- 运行相关单元测试（若有）
- 在 test-matrix.md 中勾选已通过项

### 7. 完成输出

```
## Apply 完成

**变更：** editor-asset-<kebab-type>
**资产类型：** <RegistryType>

### 本次实现
- [x] ...

### 产出物
- operation-matrix.md（已同步 Implement）
- test-matrix.md
- tasks.md

请按 test-matrix.md 完成手动验收。
```

---

## 护栏

- 以 **operation-matrix.md 为规格**；Implement 与用户修改冲突时以 matrix 为准
- 不实现 Enabled=`[ ]` 的项，除非用户显式扩大范围
- 混合多选 / 文件夹规则改动必须同时更新 SR-* 相关测试行
- 每个 OpId 的改动应能追溯到 matrix 中的一行

---

## 与 OpenSpec 的关系

| 阶段 | OpenSpec 类比 | 本流程 |
|------|---------------|--------|
| 探索规格 | `/opsx-propose` | `/UE-Editor-AssetExplore` |
| 实现 | `/opsx:apply` | `/UE-Editor-AssetApply` |
| 归档 | `/opsx:archive` | 手动归档 `editor-asset-*` 或合并到主 spec |

Editor 资产交互变更**独立**于 `add-*-pipeline` 功能变更；可并行存在。
