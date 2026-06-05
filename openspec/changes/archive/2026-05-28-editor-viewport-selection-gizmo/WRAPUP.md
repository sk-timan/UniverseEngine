# 收尾引导：editor-viewport-selection-gizmo

## 当前状态（2026-05-28）

| 维度 | 状态 |
|------|------|
| 实现任务 §1–§11 | **已完成**（见 `tasks.md`） |
| 手工验收 §12 | **已完成**（2026-05-28） |
| 主规范 `openspec/specs/` | **已同步** 三个 capability |
| 变更归档 | **待执行** — 可运行 `openspec-cn archive editor-viewport-selection-gizmo` |

## 建议收尾流程

### 第一步：手工验收（约 15 分钟）

在 `build\Debug\OpenSpecTest.exe` 中按 `tasks.md` **§12** 逐项操作，通过后把对应 `- [ ]` 改为 `- [x]`。

**重点回归项（会话已修）：**

1. **12.3** — 选大 StaticMesh（如蜘蛛），同一点快速左键 10+ 次，FPS 不应骤降  
2. **12.6 / 12.7** — 拖拽 Gizmo 移动物体时 FPS 应接近 60（非稳定 30）；同点连点 Gizmo 亦不应骤降  
3. **12.8** — 编辑 → 性能，切换 Median/SAH，拾取仍命中正确物体  

### 第二步：验证实现与规范一致（可选 CLI）

```bash
openspec-cn verify --change editor-viewport-selection-gizmo
```

或在 Cursor 中说：「对 editor-viewport-selection-gizmo 运行 openspec verify」。

### 第三步：归档变更

全部 §12 勾选后：

```bash
openspec-cn archive editor-viewport-selection-gizmo
```

归档会把变更移入 `openspec/changes/archive/`；主规范已在 `openspec/specs/` 中，无需再 sync。

## 规范与代码映射（速查）

| 能力 | 主规范 | 关键代码 |
|------|--------|----------|
| 拾取 | `openspec/specs/editor-viewport-picking/spec.md` | `EditorPicking.cpp`, `EditorTriangleBvh.cpp`, `EditorPickBvh.cpp` |
| Gizmo | `openspec/specs/editor-transform-gizmo/spec.md` | `EditorTransformGizmo.cpp`, `GameApp::TickEditorInteraction` |
| 性能设置 | `openspec/specs/editor-performance-settings/spec.md` | `EditorPerformanceDialog`, `EditorPerformanceStore`, `GameApp` 性能 API |

## 已知非目标（勿在收尾中扩大范围）

- GPU / ID Buffer 拾取、遮挡感知、多选  
- Undo/Redo、吸附、局部/世界 Gizmo 空间切换  
