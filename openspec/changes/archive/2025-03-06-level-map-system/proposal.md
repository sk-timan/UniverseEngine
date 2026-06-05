# 关卡地图系统

## 变更概述

实现关卡持久化与菜单栏功能，支持地图数据的保存与加载，完善 World/Level/Actor 生命周期管理。

## 能力

### 能力 1：关卡持久化

Level 必须支持将当前关卡内的 Actor 集合及其状态序列化为可持久化格式，并在加载时正确恢复。

- 规范：`specs/level-persistence/spec.md`

### 能力 2：UI 菜单栏

在 MainWindow 顶部添加工具栏菜单，提供新建、打开、保存地图的交互入口。

- 规范：`specs/ui-menu-bar/spec.md`

## 影响

此项变更将扩展现有的 World/Level/Actor 生命周期系统，添加序列化与反序列化能力，并在 Qt UI 层添加菜单交互。需要修改以下模块：

- **数据层**：新增 Level 序列化工具类
- **核心层**：扩展 ULevel、UWorld 接口
- **应用层**：扩展 GameApp 地图管理接口
- **UI 层**：在 MainWindow 添加菜单栏组件

## 约束

- MVP 阶段使用 JSON 作为序列化格式
- 不改变现有的 Actor 基类接口
- 菜单栏使用 Qt 原生组件
