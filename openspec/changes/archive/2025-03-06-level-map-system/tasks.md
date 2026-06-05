# 关卡地图系统 - 任务清单

## 阶段 1：数据结构与序列化

- [x] **T1.1** 创建 `FVector3` 序列化辅助函数
  - 新建 `OpenSpecTest/src/world/Vector3Serialization.h`
  - 定义 FVector3 的 JSON 序列化/反序列化辅助函数

- [x] **T1.2** 创建 `FActorSaveData` 数据结构
  - 新建 `OpenSpecTest/src/world/ActorSaveData.h`
  - 定义 Actor 保存数据结构（ActorId, Name, Type, Transform）

- [x] **T1.3** 创建 `FLevelSaveData` 数据结构
  - 新建 `OpenSpecTest/src/world/LevelSaveData.h`
  - 定义 Level 保存数据结构（LevelId, Actors, Metadata）

## 阶段 2：ULevel 持久化扩展

- [x] **T2.1** 扩展 `ULevel` 保存接口
  - 修改 `OpenSpecTest/src/world/Level.h`
  - 添加 `SaveToFile()` 和 `GetSaveData()` 方法声明

- [x] **T2.2** 实现 `ULevel` 保存功能
  - 修改 `OpenSpecTest/src/world/Level.cpp`
  - 实现 `SaveToFile()` 方法，将 Actor 数据序列化为 JSON

- [x] **T2.3** 扩展 `ULevel` 加载接口
  - 修改 `Level.h`
  - 添加 `LoadFromFile()` 方法声明

- [x] **T2.4** 实现 `ULevel` 加载功能
  - 修改 `Level.cpp`
  - 实现 `LoadFromFile()` 方法，反序列化 JSON 并恢复 Actor

## 阶段 3：UWorld 持久化管理

- [x] **T3.1** 扩展 `FLevelDefinition` 结构
  - 修改 `OpenSpecTest/src/world/World.h`
  - 添加 `SaveDataPath` 字段用于关联持久化文件

- [x] **T3.2** 扩展 `UWorld` 注册接口
  - 修改 `World.h`
  - 添加 `RegisterLevelDefinitionWithSavePath()` 方法声明

- [x] **T3.3** 实现 `UWorld` 关卡定义管理
  - 修改 `World.cpp`
  - 实现带保存路径的关卡定义注册

- [x] **T3.4** 添加获取关卡保存路径接口
  - 修改 `World.h/cpp`
  - 添加 `GetLevelSavePath()` 方法

## 阶段 4：GameApp 地图管理

- [x] **T4.1** 扩展 `GameApp` 创建关卡接口
  - 修改 `OpenSpecTest/src/app/GameApp.h`
  - 添加 `CreateNewLevel()` 方法声明

- [x] **T4.2** 实现 `GameApp` 创建关卡
  - 修改 `GameApp.cpp`
  - 实现创建新关卡并设置保存路径

- [x] **T4.3** 扩展 `GameApp` 加载接口
  - 修改 `GameApp.h`
  - 添加 `LoadLevelFromFile()` 方法声明

- [x] **T4.4** 实现 `GameApp` 文件加载
  - 修改 `GameApp.cpp`
  - 实现从 JSON 文件加载关卡数据

- [x] **T4.5** 扩展 `GameApp` 保存接口
  - 修改 `GameApp.h`
  - 添加 `SaveCurrentLevel()` 和 `SaveCurrentLevelToDefault()` 方法声明

- [x] **T4.6** 实现 `GameApp` 文件保存
  - 修改 `GameApp.cpp`
  - 实现保存当前关卡到文件

- [x] **T4.7** 确保 maps 目录存在
  - 在 `GameApp::InitializeDataDrivenResources()` 中添加 maps 目录创建逻辑

## 阶段 5：UI 菜单栏

- [x] **T5.1** 添加菜单栏头文件引用
  - 修改 `OpenSpecTest/src/ui/MainWindow.h`
  - 添加 `#include <QMenuBar>`, `#include <QMenu>`, `#include <QAction>`

- [x] **T5.2** 扩展 MainWindow 成员变量
  - 修改 `MainWindow.h`
  - 添加菜单栏和菜单项指针成员

- [x] **T5.3** 添加菜单栏槽函数声明
  - 修改 `MainWindow.h`
  - 添加 `OnNewMapClicked()`, `OnOpenMapClicked()`, `OnSaveMapClicked()`, `OnSaveAsMapClicked()`, `OnExitClicked()`

- [x] **T5.4** 实现菜单栏构建
  - 修改 `MainWindow.cpp`
  - 添加 `BuildMenuBar()` 方法，创建"文件"菜单及子菜单项

- [x] **T5.5** 实现菜单槽函数
  - 修改 `MainWindow.cpp`
  - 实现各菜单项的点击处理函数

- [x] **T5.6** 添加快捷键绑定
  - 在 `BuildMenuBar()` 中为各菜单项设置 `QKeySequence`

## 阶段 6：整合与测试

- [x] **T6.1** 更新 CMakeLists.txt
  - 添加新创建的头文件和源文件到构建系统（自动通过 GLOB 收集）

- [x] **T6.2** 测试新建地图功能
  - 运行应用，点击"文件 -> 新建地图"，验证关卡创建

- [x] **T6.3** 测试保存地图功能
  - 创建关卡后点击"文件 -> 保存地图"，验证 JSON 文件生成

- [x] **T6.4** 测试加载地图功能
  - 重启应用，点击"文件 -> 打开地图"，验证关卡数据恢复

- [x] **T6.5** 测试另存为功能
  - 验证可以将关卡另存为不同文件

- [x] **T6.6** 测试退出功能
  - 验证"文件 -> 退出"可以正常关闭应用
