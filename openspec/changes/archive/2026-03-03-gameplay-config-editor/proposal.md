## 为什么

当前编辑面板中的钓组配置仍是占位信息，无法驱动真实的配置读写，导致编辑体验与运行时行为脱节。与此同时，`gameplay.json` 的现有结构与加载逻辑存在潜在 schema 不一致风险，现在统一读写路径可以降低后续迭代成本并避免配置漂移。

## 变更内容

- 将 `gameplay` 配置从“仅启动时读取”扩展为“可在 Qt 编辑面板中可视化编辑并保存到文件”。
- 建立统一的 JSON 读写路径（基于通用 JSON 库），替换脆弱的手写字符串解析路径。
- 明确并固定 `gameplay.json` 的嵌套 schema（`world`、`economy`）。
- 增加保存前字段校验与错误反馈，支持“应用/重置/脏状态”交互闭环。
- 提供旧格式读取兼容（若存在），并在保存时收敛为新 schema。

## 功能 (Capabilities)

### 新增功能
- `gameplay-config-editor`: 提供 `gameplay` 配置的加载、编辑、校验与落盘能力，确保 UI、内存模型与文件内容一致。

### 修改功能
- （无）

## 影响

- 受影响代码：
  - `OpenSpecTest/src/ui/MainWindow.*`（编辑面板控件与交互状态）
  - `OpenSpecTest/src/data/ResourceLoader.*`（配置加载路径与兼容读取）
  - `OpenSpecTest/CMakeLists.txt`（JSON 依赖接入与链接配置）
  - 可能新增配置存储/校验模块（如 `OpenSpecTest/src/data/*Config*`）
- 受影响数据文件：
  - `OpenSpecTest/data/config/gameplay.json`（写入格式统一为新 schema）
- 受影响行为：
  - 应用启动时配置读取逻辑
  - 编辑面板从静态占位转为真实可编辑流程
- 兼容性：
  - 不引入 BREAKING API；通过读取兼容降低旧配置迁移风险。
