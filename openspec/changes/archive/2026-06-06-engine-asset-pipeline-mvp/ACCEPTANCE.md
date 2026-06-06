# engine-asset-pipeline-mvp — 验收 Checklist

对应 `tasks.md` §4 手动验收项。**已全部通过**（2026-06-06）。

**环境：** Debug 或 Release 均可；建议先用一个大 StaticMesh（10 万+ 顶点）做 4.4 / 4.5。

**可执行文件：** `out/build/x64-Debug/OpenSpecTest/OpenSpecTest.exe`

**Content 根目录：** `OpenSpecTest/Content/`（或运行时 `GProjectContentDirectory`）

---

## 自动化（可选前置）

- [x] 运行 `OpenSpecTest_AssetSerializerTests`：StaticMesh Save/Load 往返通过  
  ```powershell
  out\build\x64-Debug\OpenSpecTest\OpenSpecTest_AssetSerializerTests.exe
  ```

---

## 4.1 持久化闭环

**目标：** Import → 写盘 → 保存关卡 → 重启 → Load 后 Mesh 正确显示。

| 步骤 | 操作 | 通过 |
|------|------|------|
| 1 | 启动 Editor，打开或新建关卡 | ☑ |
| 2 | 菜单 **导入...**，选择 FBX/OBJ，指定 Content 路径（如 `Meshes/Test/MyModel`），完成导入 | ☑ |
| 3 | 确认 `Content/Meshes/Test/MyModel.uasset` 与 `MyModel.uasset.meta` 存在 | ☑ |
| 4 | 菜单 **加载模型...**，选择刚生成的 `.uasset`，Spawn 到场景 | ☑ |
| 5 | 保存关卡 JSON | ☑ |
| 6 | **完全关闭** Editor 后重新启动 | ☑ |
| 7 | Load 同一关卡，Mesh 几何与位置正确、无报错 | ☑ |
| 8 | Level JSON 中 mesh 引用为 `mesh_asset_path`（SoftObjectPath），非 `.fbx` 路径 | ☑ |

**Spec：** `level-asset-reference` — 重启持久化验证

---

## 4.2 Level 加载无 Assimp

**目标：** 加载已保存关卡时，运行路径不调用 Assimp。

| 步骤 | 操作 | 通过 |
|------|------|------|
| 1 | 在 `MeshImporter.cpp` 的 `aiImportFile` / Import 入口设断点，或临时加日志 | ☑ |
| 2 | 重启 Editor，**仅 Load 关卡**（不要点「导入」） | ☑ |
| 3 | 断点/日志 **未触发** Assimp Import | ☑ |
| 4 | 地图模型（若有）同样仅通过 uasset Load | ☑ |

**Spec：** `level-asset-reference` — 禁止 Assimp 回退

---

## 4.3 Reimport

**目标：** guid / asset_path 不变，几何更新，视口刷新。

| 步骤 | 操作 | 通过 |
|------|------|------|
| 1 | 记录已有 uasset 的 `header.guid`（用文本编辑器打开 header JSON 段，或 Registry 查询） | ☑ |
| 2 | 修改源 FBX（或换同名不同几何的文件） | ☑ |
| 3 | 对同一资产执行 **Reimport**（Import 覆盖或 Reimport 入口） | ☑ |
| 4 | `guid` 与 `asset_path` **不变** | ☑ |
| 5 | 视口 Mesh 几何已更新 | ☑ |
| 6 | `.uasset.meta` 的 `source_timestamp` 已更新 | ☑ |

**Spec：** `asset-import` — Reimport 保留身份 / 运行时刷新

---

## 4.4 二进制格式与体积

**目标：** 新 Import 的 StaticMesh 为 `UAST` 容器，体积小于旧 JSON。

| 步骤 | 操作 | 通过 |
|------|------|------|
| 1 | 对新模型执行 **导入...**（不要加载旧 JSON uasset） | ☑ |
| 2 | 用十六进制查看器或 `Format-Hex -Count 4` 确认文件头为 `55 41 53 54`（`UAST`） | ☑ |
| 3 | 同模型若仍有旧 JSON uasset，对比文件大小：二进制 **明显更小**（通常数量级差异） | ☑ |

**PowerShell 示例：**
```powershell
Format-Hex -Path OpenSpecTest\Content\Meshes\...\Model.uasset -Count 4
(Get-Item OpenSpecTest\Content\Meshes\...\Model.uasset).Length
```

**Spec：** `asset-load` — 新 Import 写入二进制 uasset

---

## 4.5 Load 性能

**目标：** 大模型从磁盘 Load 到可用几何 **< 2s**（内存缓存为空）。

| 步骤 | 操作 | 通过 |
|------|------|------|
| 1 | 完全关闭 Editor（清空 ResourceRegistry 缓存） | ☑ |
| 2 | 启动后 **加载模型...**，选择二进制 `.uasset`，计时至 Mesh 出现在视口 | ☑ |
| 3 | 主观或秒表：**< 2s**（旧 JSON 同模型通常为数秒级） | ☑ |
| 4 | （可选）若有旧 JSON 同模型，重启后再 Load 一次对比 | ☑ |

**Spec：** `asset-load` — 二进制 uasset Load 性能

---

## 4.6 旧 JSON 向后兼容

**目标：** 无 `UAST` magic 的旧 uasset 仍可 Load。

| 步骤 | 操作 | 通过 |
|------|------|------|
| 1 | 准备或保留一份 **旧版纯 JSON** uasset（文件开头为 `{`，非 `UAST`） | ☑ |
| 2 | 重启 Editor，**加载模型...** 选择该文件 | ☑ |
| 3 | Mesh 正常显示，顶点/索引/Section 无异常 | ☑ |
| 4 | 无崩溃、无 Assimp 回退 | ☑ |

**Spec：** `asset-load` — 向后兼容旧 JSON uasset

> 若仓库中已无旧 JSON uasset，可从 git 历史恢复一份，或用旧版 Serializer 临时写一份测试文件。

---

## 4.7 Import / Load 菜单分离

**目标：** 「导入」只写盘；「加载模型」才 Spawn 到关卡。

| 步骤 | 操作 | 通过 |
|------|------|------|
| 1 | **导入...** → 选源文件 → 完成后场景 **无新 Actor**（或符合设计：不自动 Spawn） | ☑ |
| 2 | Content 目录已出现 uasset/meta | ☑ |
| 3 | **加载模型...** → 选 `.uasset` → 指定 Transform → 场景中 **出现 Actor** | ☑ |
| 4 | 导入对话框在 Import 模式显示 Content 路径；Load 模式显示 Transform | ☑ |

**Spec：** `level-asset-reference` — 导入 / 从 uasset 加载分离

---

## 4.8 / 4.9（已完成）

- [x] `WRAPUP.md` — 旧关卡迁移、二进制格式、Editor 工作流
- [x] `.gitignore` — `OpenSpecTest/Content/` 白名单

---

## 验收结论

| 编号 | 项 | 结果 | 验收人 | 日期 |
|------|----|------|--------|------|
| 4.1 | 持久化闭环 | ☑ 通过 | 用户 | 2026-06-06 |
| 4.2 | 无 Assimp Load | ☑ 通过 | 用户 | 2026-06-06 |
| 4.3 | Reimport | ☑ 通过 | 用户 | 2026-06-06 |
| 4.4 | UAST + 体积 | ☑ 通过 | 用户 | 2026-06-06 |
| 4.5 | Load < 2s | ☑ 通过 | 用户 | 2026-06-06 |
| 4.6 | JSON 兼容 | ☑ 通过 | 用户 | 2026-06-06 |
| 4.7 | 菜单分离 | ☑ 通过 | 用户 | 2026-06-06 |

**备注 / 失败现象：** 无
