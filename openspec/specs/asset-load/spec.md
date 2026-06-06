## 目的

定义 uasset 反序列化、AssetManager Load 路径与二进制格式约束；运行时 Load 禁止 Assimp。

## 需求

### 需求:AssetManager 必须 Load uasset 为 UObject

系统必须通过 `UAssetManager::GetOrLoad` 根据 SoftObjectPath 加载引擎资产；Load 路径禁止调用 Assimp。

#### 场景:Load StaticMesh uasset

- **当** 请求 Load `Meshes/Characters/Soldier.Soldier`
- **那么** 系统必须从 `Content/Meshes/Characters/Soldier.uasset` 反序列化
- **那么** 必须返回有效的 `UStaticMesh` 且 `HasResidentGeometryData()` 为 true
- **那么** 禁止读取任何 fbx/gltf 源文件

#### 场景:Load SkeletalMesh uasset

- **当** 请求 Load 类型为 SkeletalMesh 的 SoftObjectPath
- **那么** 系统必须反序列化为 `USkeletalMesh` 并包含 Section 与骨骼数据

#### 场景:Load 失败返回错误

- **当** uasset 文件不存在或格式/版本无效
- **那么** 系统必须返回明确错误信息
- **那么** 禁止静默回退到 Assimp Import

### 需求:StaticMesh uasset 必须使用二进制 payload

系统必须在 Save StaticMesh 时写入二进制容器格式；Load 时必须优先解析二进制 payload，禁止对大 mesh 构建完整 JSON DOM。

#### 场景:新 Import 写入二进制 uasset

- **当** Import Factory 成功 Save StaticMesh uasset
- **那么** 磁盘文件必须以 magic `UAST` 开头
- **那么** payload_format 必须为 StaticMesh binary（值为 1）
- **那么** header 仍为 JSON，且 guid/asset_path/type 字段正确

#### 场景:二进制 uasset Load 性能

- **当** 用户 Load 含 10 万级以上顶点的二进制 StaticMesh uasset（内存缓存为空）
- **那么** 从读盘到 `HasResidentGeometryData()==true` 的总耗时应明显低于旧 JSON uasset（目标 < 2s）
- **那么** Load 路径禁止对 payload 执行 `nlohmann::json::parse` 解析顶点数组

#### 场景:向后兼容旧 JSON uasset

- **当** 磁盘上存在旧版纯 JSON uasset（无 `UAST` magic，顶层含 `header` + `object`）
- **那么** Load 必须仍能反序列化为正确的 UStaticMesh
- **那么** 几何数据（顶点数、索引数、Section）必须与 Save 前一致

#### 场景:Reimport 升级 JSON 为二进制

- **当** 用户对旧 JSON uasset 执行 Reimport
- **那么** 写入的 uasset 必须升级为二进制容器格式
- **那么** header.guid 与 asset_path 必须不变

### 需求:uasset 必须完整序列化几何数据

`UStaticMesh` 与 `USkeletalMesh` 的 Serialize/Deserialize 必须支持 CPU 侧几何往返（不含 GPU 句柄）。

#### 场景:StaticMesh 往返

- **当** 对含顶点、索引、Section、Bounds 的 UStaticMesh 执行 Save 再 Load
- **那么** Load 后的顶点数、索引数、Section 数量与 Bounds 必须与 Save 前一致

#### 场景:SkeletalMesh 往返

- **当** 对含骨骼与蒙皮权重的 USkeletalMesh 执行 Save 再 Load
- **那么** Load 后骨骼数量、Section 蒙皮数据必须与 Save 前一致

### 需求:Load 后必须注册到 ResourceRegistry

系统必须在成功 Load uasset 后将资产注册到 `ResourceRegistry`，供 Component 引用。

#### 场景:GetOrLoad 缓存

- **当** 同一 SoftObjectPath 第二次调用 GetOrLoad
- **那么** 系统必须返回已 Load 的实例，禁止重复读盘（除非显式 Reload/Reimport）

### 需求:GPU 资源在 Load 后按需 Upload

uasset 不得包含 GPU VB/IB 句柄；Load 后由渲染层在需要绘制时 Upload。

#### 场景:Load 不立即创建 GPU 缓冲

- **当** AssetManager 完成 uasset Load
- **那么** Section 的 VertexBufferGPU/IndexBufferGPU 可为 null
- **那么** Component CreateRenderState 或等效路径必须负责 Upload
