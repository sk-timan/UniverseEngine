# UE 编辑器资产操作工作流

类似 OpenSpec 的两阶段命令，用于**新增资产类型**时系统化定制 Content Browser 交互。

## 命令

| 命令 | Skill | 产出 |
|------|-------|------|
| `/UE-Editor-AssetExplore <资产>` | [ue-editor-asset-explore](ue-editor-asset-explore/SKILL.md) | `operation-matrix.md` |
| `/UE-Editor-AssetApply [变更名]` | [ue-editor-asset-apply](ue-editor-asset-apply/SKILL.md) | 代码 + `test-matrix.md` + `tasks.md` |

## 目录结构

```
openspec/changes/editor-asset-<type>/          # 进行中
openspec/changes/archive/YYYY-MM-DD-editor-asset-<type>/  # 已验收归档
├── .ue-editor-asset.yaml
├── operation-matrix.md    ← Explore
├── test-matrix.md         ← Apply + 验收
├── tasks.md               ← Apply
├── ACCEPTANCE.md          ← 验收记录（收尾）
└── WRAPUP.md              ← 收尾说明
```

## 参考

- [operation-catalog.md](operation-catalog.md) — 必扫操作清单
- [templates/](templates/) — 表格模板
- [examples/texture2d-operation-matrix.md](examples/texture2d-operation-matrix.md) — Texture2D 样例

## 快速开始

```
/UE-Editor-AssetExplore 新建的 Material 资产
# 审阅 openspec/changes/editor-asset-material/operation-matrix.md
# 验收完成后归档至 openspec/changes/archive/YYYY-MM-DD-editor-asset-material/

/UE-Editor-AssetApply editor-asset-material
# 实现并对照 test-matrix.md 验收
```
