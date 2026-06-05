# 渲染体系架构规范

本文档定义渲染组件（Component）与渲染资产（Asset）体系的完整技术规范，涵盖 Actor 组件系统、空间组件、渲染基元、网格组件、蒙皮组件、资产系统、渲染提交流程。所有规范遵循 Data / Logic / Instance 分离原则。

---

## 1. Actor 组件系统（Actor Component System）

### 1.1 目的

为 Actor 提供组件系统能力，支持通过可挂载的功能单元扩展 Actor 行为，实现组件化架构。

### 1.2 需求

#### 需求：Actor 必须支持组件挂载

Actor 必须提供组件管理能力，允许在运行时动态添加、移除组件。

**场景：创建并挂载组件到 Actor**

- **当** 用户创建 UActorComponent 实例并调用 AddComponent
- **那么** 系统必须将组件添加到 Actor 的组件列表，并建立组件到 Actor 的所有者关联

**场景：从 Actor 移除组件**

- **当** 用户调用 RemoveComponent 移除指定组件
- **那么** 系统必须从 Actor 的组件列表中移除该组件，并清除所有者关联

#### 需求：Actor 必须提供根组件

Actor 必须拥有根组件（RootComponent），用于管理 Actor 在场景中的空间变换。

**场景：设置 Actor 根组件**

- **当** 用户调用 SetRootComponent 设置根组件
- **那么** 系统必须将指定组件设为 Actor 的根组件，并更新其所有者为当前 Actor

**场景：获取 Actor 根组件**

- **当** 用户调用 GetRootComponent 获取根组件
- **那么** 系统必须返回当前设置的根组件，若未设置则返回 nullptr

#### 需求：Actor 必须支持按类型查找组件

**场景：按类型查找组件**

- **当** 用户调用 FindComponentByClass<T> 查找指定类型的组件
- **那么** 系统必须返回第一个匹配类型的组件实例，若不存在则返回 nullptr

#### 需求：组件必须支持生命周期回调

**场景：组件注册回调**

- **当** 组件被添加到 Actor 时
- **那么** 系统必须调用组件的 OnRegister 回调

**场景：组件注销回调**

- **当** 组件被从 Actor 移除时
- **那么** 系统必须调用组件的 OnUnregister 回调

**场景：组件初始化**

- **当** 组件所在 Actor 完成初始化后
- **那么** 系统必须调用组件的 Initialize 回调

#### 需求：组件必须支持 Tick 更新

**场景：组件 Tick 更新**

- **当** Actor 所在 Level 执行 Tick 时
- **那么** 系统必须遍历 Actor 所有组件，调用已启用 Tick 的组件的 Tick 方法

---

## 2. Scene 空间组件（Scene Component）

### 2.1 目的

为组件提供场景空间变换能力，支持父子层级结构与相对/世界变换计算，实现场景树组织。

### 2.2 需求

#### 需求：SceneComponent 必须支持相对变换

SceneComponent 必须能够设置和获取相对于父组件的变换（位置、旋转、缩放）。

**场景：设置相对位置**

- **当** 用户调用 SetRelativeLocation 设置相对位置
- **那么** 系统必须更新组件的相对位置，并标记世界变换为脏

**场景：设置相对旋转**

- **当** 用户调用 SetRelativeRotation 设置相对旋转
- **那么** 系统必须更新组件的相对旋转，并标记世界变换为脏

**场景：设置相对缩放**

- **当** 用户调用 SetRelativeScale3D 设置相对缩放
- **那么** 系统必须更新组件的相对缩放，并标记世界变换为脏

**场景：设置完整相对变换**

- **当** 用户调用 SetRelativeTransform 设置完整相对变换
- **那么** 系统必须更新相对变换的所有分量，并标记世界变换为脏

#### 需求：SceneComponent 必须支持世界变换计算

**场景：获取世界变换**

- **当** 用户调用 GetWorldTransform 获取世界变换
- **那么** 系统必须递归计算所有父组件的相对变换，返回组合后的世界变换

**场景：缓存世界变换**

- **当** 世界变换为脏状态且被查询时
- **那么** 系统必须计算并缓存世界变换，避免重复计算

#### 需求：SceneComponent 必须支持父子 Attach

**场景：Attach 到父组件**

- **当** 用户调用 AttachToComponent 将组件 A 挂载到组件 B
- **那么** 组件 A 的父指针必须指向 B，B 的子列表必须包含 A

**场景：从父组件 Detach**

- **当** 用户调用 DetachFromComponent 解除挂载
- **那么** 组件的父指针必须置空，父组件的子列表必须移除该组件

**场景：获取子组件**

- **当** 用户调用 GetChildComponents 获取所有子组件
- **那么** 系统必须返回当前组件的所有直接子组件

#### 需求：SceneComponent 必须支持变换更新回调

**场景：变换更新回调**

- **当** 组件的相对变换或父组件变换发生改变
- **那么** 系统必须调用 OnUpdateTransform 虚函数，供子类扩展

---

## 3. 渲染基元组件（Primitive Component）

### 3.1 目的

为组件提供可渲染/可碰撞的几何体抽象，支持可见性控制、包围体计算，为后续渲染代理扩展提供基类。

### 3.2 需求

#### 需求：PrimitiveComponent 必须支持可见性控制

**场景：设置可见性**

- **当** 用户调用 SetVisibility(true) 设置为可见
- **那么** 系统必须将可见性标志设为 true，组件参与渲染

**场景：设置不可见**

- **当** 用户调用 SetVisibility(false) 设置为不可见
- **那么** 系统必须将可见性标志设为 false，组件不参与渲染

**场景：查询可见性**

- **当** 用户调用 IsVisible 查询当前可见状态
- **那么** 系统必须返回当前的可见性标志

#### 需求：PrimitiveComponent 必须支持剔除距离

**场景：设置剔除距离**

- **当** 用户调用 SetCullDistance 设置剔除距离
- **那么** 系统必须存储剔除距离值

**场景：获取剔除距离**

- **当** 用户调用 GetCullDistance 获取剔除距离
- **那么** 系统必须返回当前设置的剔除距离

#### 需求：PrimitiveComponent 必须提供包围体

**场景：获取包围体**

- **当** 用户调用 GetBounds 获取包围体
- **那么** 系统必须返回包含 Origin（中心点）、Extent（半长）、SphereRadius（包围球半径）的 FBounds 结构

#### 需求：PrimitiveComponent 必须支持渲染状态管理

**场景：创建渲染状态**

- **当** 组件首次需要渲染时
- **那么** 系统必须调用 CreateRenderState 创建渲染状态，分配 GPU 资源

**场景：更新渲染状态**

- **当** 组件的属性（变换、材质等）发生变化时
- **那么** 系统必须调用 UpdateRenderState 更新渲染状态

**场景：销毁渲染状态**

- **当** 组件被销毁或不再需要渲染时
- **那么** 系统必须调用 DestroyRenderState 释放 GPU 资源

---

## 4. 网格组件基类（Mesh Component）

### 4.1 目的

为组件提供绑定网格资源的能力，支持静态网格和蒙皮网格的资源引用、材质覆盖、LOD 控制。

### 4.2 需求

#### 需求：MeshComponent 必须支持网格资源引用

**场景：设置网格资源 ID**

- **当** 用户调用 SetMeshAssetId 设置资源标识符（如 "meshes/building/house"）
- **那么** 系统必须存储资源标识符，并触发资源解析

**场景：获取网格资源 ID**

- **当** 用户调用 GetMeshAssetId 获取资源标识符
- **那么** 系统必须返回当前设置的资源标识符

**场景：设置网格资源对象**

- **当** 用户调用 SetMeshAsset 直接设置资源对象
- **那么** 系统必须存储资源指针，并可选择更新资源标识符

**场景：获取网格资源对象**

- **当** 用户调用 GetMeshAsset 获取资源对象
- **那么** 系统必须返回当前引用的资源对象，若未解析则返回 nullptr

#### 需求：MeshComponent 必须支持材质覆盖

**场景：设置材质覆盖**

- **当** 用户调用 SetMaterialOverride 设置指定槽位的材质覆盖
- **那么** 系统必须在材质覆盖列表中添加或更新该槽位的覆盖

**场景：清除材质覆盖**

- **当** 用户调用 ClearMaterialOverride 清除指定槽位的材质覆盖
- **那么** 系统必须从材质覆盖列表中移除该槽位的覆盖

**场景：获取材质覆盖列表**

- **当** 用户调用 GetMaterialOverrides 获取所有材质覆盖
- **那么** 系统必须返回当前所有材质覆盖的列表

#### 需求：MeshComponent 必须支持 LOD 控制

**场景：设置强制 LOD 级别**

- **当** 用户调用 SetForcedLODLevel 设置强制 LOD
- **那么** 系统必须存储 LOD 级别，渲染时使用指定 LOD（0 表示自动）

**场景：获取强制 LOD 级别**

- **当** 用户调用 GetForcedLODLevel 获取强制 LOD
- **那么** 系统必须返回当前设置的 LOD 级别

**场景：获取当前 LOD 级别**

- **当** 用户调用 GetCurrentLODLevel 获取当前计算的 LOD
- **那么** 系统必须基于组件位置与摄像机的距离计算并返回 LOD 级别

#### 需求：MeshComponent 必须重写包围体

**场景：获取网格包围体**

- **当** 用户调用 GetBounds 获取包围体
- **那么** 系统必须返回网格资源的包围体，结合组件的世界变换

---

## 5. 静态网格组件（Static Mesh Component）

### 5.1 目的

实现静态网格渲染能力，支持将 UStaticMesh 资产绑定到组件并提交渲染，实现不可变形网格的实例化绘制。

### 5.2 需求

#### 需求：StaticMeshComponent 必须引用 UStaticMesh 资产

**场景：设置静态网格资产**

- **当** 用户调用 SetMeshAsset 设置 UStaticMesh 资产对象
- **那么** 系统必须存储资产指针，并触发渲染状态重建

**场景：获取静态网格资产**

- **当** 用户调用 GetMeshAsset 获取资产对象
- **那么** 系统必须返回 UStaticMesh 类型的资产指针

#### 需求：StaticMeshComponent 必须创建渲染状态

**场景：创建顶点缓冲**

- **当** 渲染状态首次创建时
- **那么** 系统必须从 UStaticMesh 获取顶点数据，创建 D3D12 顶点缓冲

**场景：创建索引缓冲**

- **当** 渲染状态首次创建时
- **那么** 系统必须从 UStaticMesh 获取索引数据，创建 D3D12 索引缓冲

**场景：绑定材质**

- **当** 渲染状态创建时
- **那么** 系统必须为每个 Section 绑定对应的材质（或材质覆盖）

#### 需求：StaticMeshComponent 必须更新渲染状态

**场景：更新世界变换**

- **当** 组件的世界变换发生变化时
- **那么** 系统必须更新渲染状态中的世界矩阵

**场景：更新材质覆盖**

- **当** 组件的材质覆盖发生变化时
- **那么** 系统必须更新渲染状态中的材质绑定

#### 需求：StaticMeshComponent 必须销毁渲染状态

**场景：释放顶点缓冲**

- **当** 渲染状态销毁时
- **那么** 系统必须释放 D3D12 顶点缓冲资源

**场景：释放索引缓冲**

- **当** 渲染状态销毁时
- **那么** 系统必须释放 D3D12 索引缓冲资源

#### 需求：UStaticMesh 资产必须提供几何数据

**场景：设置顶点数据**

- **当** 用户调用 SetVertices 设置顶点数组
- **那么** 系统必须存储顶点数据，并更新包围体

**场景：获取顶点数据**

- **当** 用户调用 GetVertices 获取顶点数组
- **那么** 系统必须返回当前存储的顶点数据

**场景：设置索引数据**

- **当** 用户调用 SetIndices 设置索引数组
- **那么** 系统必须存储索引数据

**场景：获取索引数据**

- **当** 用户调用 GetIndices 获取索引数组
- **那么** 系统必须返回当前存储的索引数据

**场景：添加 Section**

- **当** 用户调用 AddSection 添加子网格
- **那么** 系统必须将 Section 添加到列表，更新索引范围

**场景：获取 Section**

- **当** 用户调用 GetSection 获取指定 Section
- **那么** 系统必须返回对应索引的 Section 数据

#### 需求：UStaticMesh 必须提供包围体

**场景：计算包围体**

- **当** 用户调用 GetBounds 获取包围体
- **那么** 系统必须基于所有顶点计算 AABB 和包围球

---

## 6. 蒙皮网格组件（Skinned Mesh Component）

### 6.1 目的

实现蒙皮网格渲染能力，支持骨骼驱动的网格变形，支持 GPU 蒙皮或 CPU 蒙皮路径，为骨骼动画系统提供渲染基类。

### 6.2 需求

#### 需求：SkinnedMeshComponent 必须支持骨骼变换

**场景：设置骨骼变换数组**

- **当** 用户调用 SetBoneTransforms 设置骨骼变换数组
- **那么** 系统必须存储骨骼变换数据，供渲染状态更新使用

**场景：获取骨骼变换数组**

- **当** 用户调用 GetBoneTransforms 获取骨骼变换数组
- **那么** 系统必须返回当前存储的骨骼变换数据

#### 需求：SkinnedMeshComponent 必须引用 USkeletalMesh 资产

**场景：设置骨骼网格资产**

- **当** 用户调用 SetMeshAsset 设置 USkeletalMesh 资产对象
- **那么** 系统必须存储资产指针，并触发渲染状态重建

**场景：获取骨骼网格资产**

- **当** 用户调用 GetMeshAsset 获取资产对象
- **那么** 系统必须返回 USkeletalMesh 类型的资产指针

#### 需求：SkinnedMeshComponent 必须创建蒙皮渲染状态

**场景：创建蒙皮顶点缓冲**

- **当** 渲染状态首次创建时
- **那么** 系统必须从 USkeletalMesh 获取带蒙皮权重的顶点数据，创建顶点缓冲

**场景：创建骨骼矩阵缓冲**

- **当** 渲染状态首次创建时
- **那么** 系统必须为骨骼矩阵创建常量缓冲，供 GPU 蒙皮使用

**场景：绑定骨骼到 Shader**

- **当** 渲染状态创建时
- **那么** 系统必须将骨骼索引列表绑定到 Shader，确保只有相关骨骼参与计算

#### 需求：SkinnedMeshComponent 必须更新蒙皮渲染状态

**场景：更新骨骼矩阵**

- **当** 骨骼变换发生变化时
- **那么** 系统必须更新骨骼矩阵常量缓冲

#### 需求：SkeletalMeshComponent 必须实现蒙皮绘制

**场景：GPU 蒙毛路径**

- **当** 使用 GPU 蒙皮时
- **那么** 系统必须将骨骼矩阵传入 Vertex Shader，在 Shader 中执行顶点变换

**场景：CPU 蒙毛路径**

- **当** 使用 CPU 蒙毛时
- **那么** 系统必须先在 CPU 端计算蒙毛顶点，再提交到 GPU

#### 需求：USkinnedAsset 必须提供骨骼数据

**场景：设置骨骼数组**

- **当** 用户调用 SetSkeleton 设置骨骼数组
- **那么** 系统必须存储骨骼层级结构

**场景：获取骨骼数组**

- **当** 用户调用 GetSkeleton 获取骨骼数组
- **那么** 系统必须返回骨骼数组

**场景：按名称查找骨骼索引**

- **当** 用户调用 FindBoneIndex 查找骨骼
- **那么** 系统必须返回对应名称的骨骼索引，若不存在则返回 -1

**场景：设置蒙毛顶点**

- **当** 用户调用 SetSkinVertices 设置蒙毛顶点数组
- **那么** 系统必须存储蒙毛顶点数据

**场景：获取蒙毛顶点**

- **当** 用户调用 GetSkinVertices 获取蒙毛顶点数组
- **那么** 系统必须返回蒙毛顶点数据

#### 需求：USkeletalMesh 必须提供蒙毛 Section

**场景：添加蒙毛 Section**

- **当** 用户调用 AddSection 添加蒙毛 Section
- **那么** 系统必须将 Section 添加到列表，包含骨骼索引列表

**场景：获取蒙毛 Section**

- **当** 用户调用 GetSection 获取指定 Section
- **那么** 系统必须返回包含骨骼索引的 Section 数据

---

## 7. 渲染资产系统（Render Asset System）

### 7.1 目的

为渲染资产提供统一的对象模型与资源标识机制，支持资源加载、注册、查找，实现资产的生命周期管理。

### 7.2 需求

#### 需求：渲染资产必须继承自 UStreamableRenderAsset

**场景：创建渲染资产**

- **当** 用户创建 UStaticMesh 或 USkeletalMesh 实例
- **那么** 系统必须将资产注册为 UStreamableRenderAsset 子类

#### 需求：渲染资产必须支持资源路径标识

**场景：设置资产路径**

- **当** 用户调用 SetAssetPath 设置资源路径（如 "meshes/building/house"）
- **那么** 系统必须存储相对 data 目录的资源路径

**场景：获取资产路径**

- **当** 用户调用 GetAssetPath 获取资源路径
- **那么** 系统必须返回当前存储的资源路径

#### 需求：渲染资产必须支持加载状态查询

**场景：查询加载状态**

- **当** 用户调用 GetLoadingStatus 获取加载状态
- **那么** 系统必须返回当前加载状态（Unloaded / Loading / Loaded / Failed）

#### 需求：渲染资产必须预留流式加载接口

**场景：请求加载（预留）**

- **当** 用户调用 RequestLoad 请求加载资源
- **那么** 系统必须返回成功（当前 MVP 阶段资源始终已加载）

**场景：释放资源（预留）**

- **当** 用户调用 Release 释放资源
- **那么** 系统必须返回成功（当前 MVP 阶段不做实际释放）

#### 需求：ResourceRegistry 必须支持资产注册与查找

**场景：注册资产**

- **当** 用户调用 RegisterAsset 注册资产
- **那么** 系统必须将资产添加到注册表，使用资源 ID 作为键

**场景：注销资产**

- **当** 用户调用 UnregisterAsset 注销资产
- **那么** 系统必须从注册表中移除对应资产

**场景：按 ID 查找资产**

- **当** 用户调用 FindAsset 查找资产
- **那么** 系统必须返回对应资源 ID 的资产指针，若不存在则返回 nullptr

**场景：按类型查找资产**

- **当** 用户调用 FindAsset<T> 模板方法按类型查找
- **那么** 系统必须返回对应资源 ID 且类型匹配的第一个资产

#### 需求：ResourceLoader 必须支持资产加载

**场景：加载静态网格**

- **当** 用户调用 LoadStaticMesh 加载静态网格
- **那么** 系统必须使用 Assimp 解析模型文件，创建 UStaticMesh 资产并返回

**场景：加载骨骼网格**

- **当** 用户调用 LoadSkeletalMesh 加载骨骼网格
- **那么** 系统必须使用 Assimp 解析模型文件，创建 USkeletalMesh 资产并返回

**场景：加载失败处理**

- **当** 资源加载失败（文件不存在、格式错误等）
- **那么** 系统必须返回 nullptr，并通过错误消息参数输出错误信息

#### 需求：渲染资产必须支持序列化

**场景：序列化资产数据**

- **当** 用户调用 Serialize 序列化资产
- **那么** 系统必须将资产路径、顶点、索引、Section 等数据写入 JSON

---

## 8. 渲染提交流程（Render Collector）

### 8.1 目的

实现渲染提交系统，支持从场景中收集可见的渲染基元，构建绘制命令并提交给渲染器。

### 8.2 需求

#### 需求：RenderCollector 必须能够收集可见基元

**场景：添加渲染基元**

- **当** 用户调用 AddPrimitive 添加渲染基元
- **那么** 系统必须将基元添加到可见列表

**场景：收集所有可见基元**

- **当** 用户调用 CollectVisiblePrimitives 收集可见基元
- **那么** 系统必须遍历所有 Actor 的所有 PrimitiveComponent，筛选可见的组件

#### 需求：RenderCollector 必须能够构建绘制命令

**场景：构建网格绘制命令**

- **当** 用户调用 BuildRenderCommands 构建绘制命令
- **那么** 系统必须为每个可见的 MeshComponent 生成 FMeshDrawCommand，包含网格资源、世界变换、材质等信息

**场景：过滤不可见基元**

- **当** 构建绘制命令时
- **那么** 系统必须跳过不可见或超出剔除距离的基元

#### 需求：RenderCollector 必须能够提交给渲染器

**场景：提交绘制命令**

- **当** 用户调用 SubmitDrawCommands 提交绘制命令
- **那么** 系统必须遍历所有绘制命令，调用渲染器的绘制接口

#### 需求：World 必须集成渲染收集流程

**场景：World 渲染更新**

- **当** World 执行 Tick 时
- **那么** 系统必须调用 Render 方法，收集可见基元并提交给渲染器

#### 需求：渲染器必须支持网格绘制接口

**场景：绘制网格**

- **当** 渲染器接收到网格绘制命令
- **那么** 系统必须绑定顶点/索引缓冲，设置世界矩阵和材质，执行 DrawCall

---

## 9. 数据类型定义

### 9.1 FBounds（包围体）

```cpp
struct FBounds
{
    FVector3 Origin;      // 中心点
    FVector3 Extent;      // 半长（AABB 半尺寸）
    float SphereRadius;   // 包围球半径
};
```

### 9.2 FTransform（变换）

支持位置、旋转、缩放的组合存储，提供世界变换计算接口。

### 9.3 FMaterialOverride（材质覆盖）

```cpp
struct FMaterialOverride
{
    int32 MaterialSlot;              // 网格的材质槽位
    std::string MaterialAssetId;    // 覆盖的材质资源 ID
};
```

### 9.4 FMeshDrawCommand（绘制命令）

```cpp
struct FMeshDrawCommand
{
    UStreamableRenderAsset* MeshAsset;           // 网格资源
    FTransform WorldTransform;                  // 世界变换
    std::vector<FMaterialOverride> Materials;   // 材质覆盖
    // ... 其他渲染参数
};
```

---

## 10. 验收标准总览

### Actor 组件系统

1. 能够在 Actor 上成功添加和移除组件。
2. 组件能够正确获取其所有者 Actor。
3. Actor 能够设置和获取根组件。
4. FindComponentByClass 能够正确返回指定类型的组件。
5. OnRegister / OnUnregister / Initialize / Uninitialize 回调能够被正确触发。
6. 组件的 Tick 方法能够在 Actor Tick 时被正确调用。

### Scene 空间组件

1. 相对位置、旋转、缩放能够正确设置和获取。
2. 世界变换计算正确，父子变换能够正确叠加。
3. Attach/Detach 能够正确建立和解除父子关系。
4. 子组件遍历能够正确返回所有子组件。
5. OnUpdateTransform 回调在变换更新时被正确调用。

### 渲染基元组件

1. SetVisibility / IsVisible 能够正确控制可见性。
2. 剔除距离能够正确设置和获取。
3. GetBounds 能够返回有效的包围体数据。
4. CreateRenderState / UpdateRenderState / DestroyRenderState 能够被正确调用。

### 网格组件基类

1. SetMeshAssetId / GetMeshAssetId 能够正确设置和获取资源标识符。
2. SetMeshAsset / GetMeshAsset 能够正确设置和获取资源对象。
3. 材质覆盖能够正确设置、清除和获取。
4. LOD 强制级别和自动计算能够正确工作。

### 静态网格组件

1. StaticMeshComponent 能够正确引用 UStaticMesh 资产。
2. 渲染状态创建时能够正确生成顶点/索引缓冲。
3. 世界变换更新时能够正确更新渲染状态。
4. 材质覆盖能够正确生效。
5. UStaticMesh 能够正确存储和提供几何数据。

### 蒙皮网格组件

1. SkinnedMeshComponent 能够正确接收和存储骨骼变换。
2. SkeletalMeshComponent 能够正确引用 USkeletalMesh 资产。
3. 蒙毛渲染状态创建时能够正确生成顶点缓冲和骨骼矩阵缓冲。
4. GPU 蒙毛和 CPU 蒙毛路径能够正确执行。
5. USkeletalMesh 能够正确提供带骨骼索引的 Section。

### 渲染资产系统

1. UStaticMesh 和 USkeletalMesh 正确继承 UStreamableRenderAsset。
2. 资源路径能够正确设置和获取。
3. ResourceRegistry 能够正确注册、注销、查找资产。
4. ResourceLoader 能够加载并转换模型文件为资产。

### 渲染提交流程

1. RenderCollector 能够正确收集可见的 PrimitiveComponent。
2. 构建的绘制命令包含正确的信息。
3. 不可见或超出剔除距离的基元被正确过滤。
4. World 能够在 Tick 时触发渲染收集与提交流程。

---

**规范版本**：1.0  
**创建日期**：2026-03-12  
**最后更新**：2026-03-12
