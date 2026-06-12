## 新增需求

### 需求:Details 面板必须基于反射自动生成可编辑属性 UI
编辑器 `DetailPanelWidget`（或等价 Details 面板）必须通过遍历选中 `UObject` 的 `UClass` 属性链构建 UI，禁止为每个新增可编辑字段手工添加专用控件（试点迁移的 Transform 字段除外过渡期）。

#### 场景:EditAnywhere 属性显示控件
- **当** 用户选中带有反射注册的 `UObject`，且某属性带 `EditAnywhere` 标志
- **那么** Details 面板必须显示该属性行，且控件类型与属性类型匹配（`float`/`double`→数值框，`bool`→复选框，`FVector`→三分量框，枚举→下拉框）

#### 场景:VisibleAnywhere 只读显示
- **当** 属性仅带 `VisibleAnywhere` 而无 `EditAnywhere`
- **那么** Details 面板必须显示该属性值但禁止用户编辑

#### 场景:无编辑标志的属性不显示
- **当** 属性不带 `EditAnywhere` 或 `VisibleAnywhere`
- **那么** Details 面板必须不显示该属性

### 需求:Details 面板必须按 Category 分组
系统必须读取 `UPROPERTY` 的 `Category` 参数，将同 Category 属性放入同一 `QGroupBox`（或等价分组容器）；未指定 Category 的属性归入默认分组。

#### 场景:Transform 分组
- **当** `USceneComponent` 的 Location/Rotation/Scale 标记为 `Category="Transform"`
- **那么** Details 面板必须在「Transform」分组下展示这些字段，且数值与对象实际成员一致

### 需求:属性编辑必须写回运行时对象
用户在 Details 面板修改反射驱动控件后，系统必须立即通过 `FProperty::SetValue` 写回选中对象；写回后游戏/编辑器内依赖该字段的逻辑必须能读到新值。

#### 场景:修改 Location 后组件变换更新
- **当** 用户编辑 `USceneComponent` 的 Location 并确认
- **那么** 系统必须更新组件 Transform，且视口/Outliner 展示的变换与新区一致

### 需求:Details 面板必须支持属性搜索过滤
系统必须保留或提供搜索框，按属性名或 `DisplayName` meta 过滤可见属性行。

#### 场景:搜索隐藏不匹配项
- **当** 用户输入搜索文本
- **那么** 面板必须仅显示名称或 DisplayName 包含该文本的属性行（大小写策略与现有面板一致）

## 修改需求

无。

## 移除需求

无。
