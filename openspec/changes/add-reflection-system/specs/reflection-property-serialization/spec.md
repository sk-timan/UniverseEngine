## 新增需求

### 需求:系统必须基于 SaveGame 标志自动序列化属性
系统必须提供 `FPropertySerializer`（或等价组件），遍历 `UClass` 属性链，对带 `SaveGame` 标志的属性执行 JSON 读写；禁止为试点组件的每个字段单独编写序列化函数。

#### 场景:保存组件时写出 SaveGame 属性
- **当** `ComponentSerializer` 序列化已反射注册的 `USceneComponent` 或 `UMeshComponent`
- **那么** 输出 JSON 必须包含所有 `SaveGame` 属性，键名默认等于属性名，值类型与属性类型匹配

#### 场景:加载组件时恢复 SaveGame 属性
- **当** 从存档 JSON 反序列化上述组件且键存在、类型合法
- **那么** 系统必须通过反射将值写回对应成员，且与保存前一致

#### 场景:Transient 属性不写入存档
- **当** 属性带 `Transient` 或不含 `SaveGame`
- **那么** 序列化输出必须省略该属性

### 需求:组件序列化必须保持试点类存档兼容
对 `USceneComponent`、`UMeshComponent`、`UStaticMeshComponent` 迁移到反射序列化后，系统必须保持与现有存档字段名兼容（`Location`、`Rotation`、`Scale`、`MeshAssetPath` 等），禁止使已有测试关卡/组件存档无法加载。

#### 场景:旧格式 Location 可加载
- **当** 加载包含既有 `Location` 数组或对象格式的组件 JSON
- **那么** 系统必须正确填充 `USceneComponent::Location`（或等价反射属性）

### 需求:类型分支创建与反射字段读写分离
`ComponentSerializer` 允许保留按 `ComponentClass` 字符串选择 `NewObject` 类型的分支，但字段读写必须委托给反射序列化器，禁止在新代码中新增 `if (IsA(...)) { 手写字段 }` 模式。

#### 场景:静态网格组件 Mesh 路径往返
- **当** 序列化并反序列化带 `MeshAssetPath`（或等价 `SaveGame` 属性）的 `UStaticMeshComponent`
- **那么** 往返后网格资产路径必须与原始值一致

### 需求:序列化错误必须可诊断
当 JSON 类型与属性类型不匹配、或必填 `SaveGame` 属性缺失时，系统必须返回含属性名与组件类名的错误信息，禁止静默忽略或崩溃。

#### 场景:类型不匹配拒绝加载
- **当** 某 `SaveGame` 浮点属性在 JSON 中为字符串且无法转换
- **那么** 反序列化必须失败并报告属性名与期望类型

## 修改需求

无。

## 移除需求

无。
