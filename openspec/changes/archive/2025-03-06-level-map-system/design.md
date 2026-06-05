## 上下文

本次变更继承自 `world-level-actor-lifecycle` 变更，建立的 World/Level/Actor 三层运行时结构已验证可用。当前需要扩展以下能力：

1. **Level 持久化**：将关卡内 Actor 数据序列化保存，并在加载时恢复
2. **UI 菜单栏**：在 MainWindow 添加地图管理交互入口

本次变更的约束如下：
- 继续使用现有 JSON 格式作为序列化载体
- 不改变现有 Actor 基类接口
- 菜单栏使用 Qt 原生 QMenuBar/QMenu/QAction 组件
- 继续与现有 GameApp、UWorld、ULevel、ObjectRegistry 协同

## 目标 / 非目标

**目标：**
- 为 ULevel 添加 Save/Load 接口，支持关卡数据持久化
- 扩展 UWorld 支持关卡定义与持久化数据的关联管理
- 在 MainWindow 添加菜单栏，提供新建/打开/保存地图功能
- 建立 GameApp 到地图文件操作的完整调用链

**非目标：**
- 不实现异步加载或流式加载
- 不引入新的序列化格式（继续使用 JSON）
- 不实现完整的场景编辑器功能
- 不添加 Actor 组件系统扩展

## 决策

### 决策 1：Level 持久化采用独立 JSON 文件

- 选择：为每个关卡创建独立的 JSON 文件存储 Actor 数据
- 原因：简单直接，便于调试，与现有 gameplay.json 体系一致
- 备选方案：
  - 将关卡数据嵌入 gameplay.json：数据结构会变得复杂，不适合多关卡场景

### 决策 2：使用 nlohmann/json 进行序列化

- 选择：使用项目中已有的 JSON 库进行序列化
- 原因：保持与 GameplayConfigStore 一致的序列化方式
- 备选方案：
  - 手写 JSON 序列化：工作量大且易出错

### 决策 3：菜单栏使用 Qt 原生组件

- 选择：使用 QMenuBar + QMenu + QAction 构建菜单
- 原因：Qt 原生支持，无需额外依赖，与现有 MainWindow 风格一致
- 备选方案：
  - 自定义菜单 UI：开发成本高，不符合 MVP 快速迭代原则

### 决策 4：文件对话框使用 QFileDialog

- 选择：使用 Qt 的 QFileDialog 进行文件选择
- 原因：提供原生系统文件对话框体验，易于配置过滤器
- 备选方案：
  - 自定义文件选择器：超出 MVP 范围

### 决策 5：GameApp 作为地图管理的唯一入口

- 选择：所有地图操作（新建/加载/保存）统一通过 GameApp 接口
- 原因：保持应用层"编排者"角色，避免业务逻辑散落
- 备选方案：
  - UI 直接操作 World/Level：违反分层架构原则

## 架构设计

### 数据结构

```cpp
// 关卡保存数据
struct FActorSaveData
{
    std::string ActorId;
    std::string ActorName;
    std::string ActorType;
    FVector3 Position;
    FVector3 Rotation;
    FVector3 Scale;
};

struct FLevelSaveData
{
    std::string LevelId;
    std::string LevelName;
    std::string MapModelPath;
    std::vector<FActorSaveData> Actors;
    std::string CreatedAt;
    std::string ModifiedAt;
};
```

### 接口设计

**GameApp 新增接口：**
```cpp
// 创建新关卡
bool CreateNewLevel(const std::string& InLevelId, std::string* OutErrorMessage);

// 从文件加载关卡
bool LoadLevelFromFile(const std::filesystem::path& InFilePath, std::string* OutErrorMessage);

// 保存当前关卡到文件
bool SaveCurrentLevel(const std::filesystem::path& InFilePath, std::string* OutErrorMessage);

// 保存当前关卡到默认路径
bool SaveCurrentLevelToDefault(std::string* OutErrorMessage);
```

**UWorld 新增接口：**
```cpp
// 注册带持久化路径的关卡定义
bool RegisterLevelDefinitionWithSavePath(
    const FLevelDefinition& InDefinition,
    const std::filesystem::path& InSaveDataPath,
    std::string* OutErrorMessage);

// 获取关卡定义的保存路径
std::filesystem::path GetLevelSavePath(const std::string& InLevelId) const;
```

**ULevel 新增接口：**
```cpp
// 保存到文件
bool SaveToFile(const std::filesystem::path& InFilePath, std::string* OutErrorMessage);

// 从文件加载
bool LoadFromFile(const std::filesystem::path& InFilePath, std::string* OutErrorMessage);

// 获取保存数据
FLevelSaveData GetSaveData() const;
```

### 文件存储结构

```
data/
├── maps/                              # 地图持久化目录
│   ├── mvp_pond_01.json             # 默认地图
│   └── runtime_new_level_1.json       # 运行时创建的新地图
├── config/
│   └── gameplay.json                 # 游戏配置
└── ...
```

### 菜单结构

```
文件(F)
├── 新建地图(N)          Ctrl+N
├── 打开地图(O)          Ctrl+O
├── ─────────────────
├── 保存地图(S)          Ctrl+S
├── 另存为(A)...         Ctrl+Shift+S
├── ─────────────────
└── 退出(X)             Alt+F4
```

## 风险 / 权衡

- [序列化数据与运行时状态不一致] -> 通过明确的 SaveData 结构分离持久化数据与运行时状态
- [文件损坏导致加载失败] -> 添加 JSON 解析错误处理，返回清晰错误信息
- [菜单操作与当前关卡状态冲突] -> 在操作前检查关卡状态，必要时先保存或提示用户

## 实施计划

1. 新增 Level 序列化数据结构
2. 扩展 ULevel 接口实现 Save/Load
3. 扩展 UWorld 管理关卡定义与保存路径
4. 扩展 GameApp 地图管理接口
5. 在 MainWindow 添加菜单栏
6. 连接菜单操作到 GameApp 接口
7. 测试验证完整流程

## Open Questions

- 关卡文件是否需要版本号字段以支持未来格式迁移？
- 是否需要为关卡添加描述/名称等元数据字段？
- 地图保存目录是否需要运行时创建？
