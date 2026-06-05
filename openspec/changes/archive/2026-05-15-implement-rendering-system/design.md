# 渲染组件与资产体系设计

## 1. 概述

本文档详细描述渲染组件（Component）与渲染资产（Asset）体系的实现设计，继承自 `docs/rendering-architecture-design.md` 的框架，并与现有 `UObject`、`AActor`、`ObjectRegistry`、`Dx12Renderer`、`ResourceLoader` 对接。

### 1.1 设计目标

- **职责分离**：Asset 只存数据，Component 负责渲染实例逻辑，Actor 只负责游戏逻辑。
- **可扩展性**：预留 SceneProxy、流式加载、多线程渲染的扩展点。
- **兼容性**：与现有工程无缝对接，不破坏现有 MVP 路径。

---

## 2. UObject 扩展：Outer / Inner

### 2.1 扩展内容

为 `UObject` 添加引用树支持：

```cpp
// UObject.h 新增成员
class UObject
{
    // ... 现有成员 ...
    
    // Outer: 逻辑归属父对象（Asset 树的拥有者）
    uint64_t OuterObjectId_ = 0;
    
    // Inner: 当前对象拥有的子对象 ID 列表
    std::vector<uint64_t> InnerObjectIds_;
};
```

### 2.2 接口设计

```cpp
class UObject
{
public:
    // ... 现有接口 ...
    
    // Outer 操作
    void SetOuter(uint64_t InOuterObjectId);
    uint64_t GetOuter() const;
    
    // Inner 操作
    void AddInner(uint64_t InInnerObjectId);
    void RemoveInner(uint64_t InInnerObjectId);
    const std::vector<uint64_t>& GetInnerObjectIds() const;
    
    // 序列化扩展
    void Serialize(nlohmann::json* OutObjectJson) const override;
};
```

### 2.3 职责

- **Outer**：记录逻辑归属，如「材质库拥有材质」「Mesh 拥有 LOD」。用于序列化时的递归遍历和依赖分析。
- **Inner**：Outer 的反向关系，便于遍历子对象。
- **注意**：运行时属性查找不通过 Outer/Inner，而是通过资源注册表（ResourceRegistry）或对象 ID。

---

## 3. Component 继承体系

### 3.1 类层次

```
UObject
  └── UActorComponent (src/components/ActorComponent.h/.cpp)
        └── USceneComponent (src/components/SceneComponent.h/.cpp)
              └── UPrimitiveComponent (src/components/PrimitiveComponent.h/.cpp)
                    └── UMeshComponent (src/components/MeshComponent.h/.cpp)
                          ├── UStaticMeshComponent (src/components/StaticMeshComponent.h/.cpp)
                          └── USkinnedMeshComponent (src/components/SkinnedMeshComponent.h/.cpp)
                                └── USkeletalMeshComponent (src/components/SkeletalMeshComponent.h/.cpp)
```

### 3.2 UActorComponent

**文件**：`src/components/ActorComponent.h`、`src/components/ActorComponent.cpp`

**职责**：挂载到 Actor 上的功能单元基类。

```cpp
class UActorComponent : public UObject
{
public:
    UActorComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass);
    virtual ~UActorComponent() = default;
    
    // 归属的 Actor
    void SetOwnerActor(AActor* InOwner);
    AActor* GetOwnerActor() const;
    
    // 生命周期回调（可选实现）
    virtual void OnRegister();      // 注册到 Actor
    virtual void OnUnregister();    // 从 Actor 注销
    virtual void Initialize();      // 初始化
    virtual void Uninitialize();   // 反初始化
    virtual void Tick(float InDeltaSeconds);  // 每帧更新
    
    // 注册到 UClass 系统
    static const UClass& StaticClass();
    
private:
    AActor* OwnerActor_ = nullptr;
    bool bIsRegistered_ = false;
    bool bIsInitialized_ = false;
};
```

**关键设计**：
- 不包含空间信息（由 SceneComponent 负责）。
- 提供生命周期回调，供子类扩展渲染、物理等功能。
- 通过 `OwnerActor_` 关联到所属 Actor。

### 3.3 USceneComponent

**文件**：`src/components/SceneComponent.h`、`src/components/SceneComponent.cpp`

**职责**：具有空间变换能力的组件，管理父子层级与相对/世界变换。

```cpp
class USceneComponent : public UActorComponent
{
public:
    USceneComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass);
    virtual ~USceneComponent() = default;
    
    // --- 变换操作 ---
    void SetRelativeTransform(const FTransform& InTransform);
    const FTransform& GetRelativeTransform() const;
    
    void SetRelativeLocation(const FVector3& InLocation);
    const FVector3& GetRelativeLocation() const;
    
    void SetRelativeRotation(const FRotator3& InRotation);
    FRotator3 GetRelativeRotation() const;
    
    void SetRelativeScale3D(const FVector3& InScale);
    const FVector3& GetRelativeScale3D() const;
    
    // --- 世界变换 ---
    FTransform GetWorldTransform() const;
    FVector3 GetWorldLocation() const;
    FRotator3 GetWorldRotation() const;
    FVector3 GetWorldScale3D() const;
    
    // --- 父子层级 ---
    void AttachToComponent(USceneComponent* InParent);
    void DetachFromComponent();
    USceneComponent* GetAttachParent() const;
    void GetChildComponents(std::vector<USceneComponent*>& OutChildren) const;
    
    // --- 更新 ---
    virtual void OnUpdateTransform();  // 变换更新回调
    void MarkTransformDirty();         // 标记需要重新计算世界变换
    
    static const UClass& StaticClass();
    
private:
    void UpdateWorldTransform();
    
    FTransform RelativeTransform_;  // 相对父组件的变换
    mutable FTransform CachedWorldTransform_;  // 缓存的世界变换
    mutable bool bWorldTransformDirty_ = true;
    
    USceneComponent* AttachParent_ = nullptr;  // 父组件
    std::vector<USceneComponent*> ChildComponents_;  // 子组件
};
```

**关键设计**：

- **父子 Attach**：支持场景树构建，父子变换叠加得到世界变换。
- **脏标记**：`bWorldTransformDirty_` 用于延迟计算，避免每帧重复遍历父链。
- **FTransform**：使用 `FTransform`（矩阵或位置+旋转+缩放）存储变换，与 `AActor::Transform` 对齐但归属 Component。

### 3.4 UPrimitiveComponent

**文件**：`src/components/PrimitiveComponent.h`、`src/components/PrimitiveComponent.cpp`

**职责**：代表场景中可渲染/可碰撞的几何体基类，提供可见性、Bounds、渲染代理接口。

```cpp
class UPrimitiveComponent : public USceneComponent
{
public:
    UPrimitiveComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass);
    virtual ~UPrimitiveComponent() = default;
    
    // --- 可见性 ---
    void SetVisibility(bool bInVisible);
    bool IsVisible() const;
    
    void SetCullDistance(float InCullDistance);
    float GetCullDistance() const;
    
    // --- Bounds ---
    struct FBounds
    {
        FVector3 Origin;
        FVector3 Extent;  // 半长
        float SphereRadius;
    };
    virtual FBounds GetBounds() const;
    
    // --- 渲染代理接口（预留 SceneProxy 扩展） ---
    // 当前：直接提交渲染；后续：创建/更新 SceneProxy
    struct FPrimitiveRenderState
    {
        // 顶点/索引缓冲句柄、材质句柄、世界矩阵等
        // 后续可替换为 SceneProxy*
    };
    
    virtual void CreateRenderState(FPrimitiveRenderState* OutRenderState);
    virtual void UpdateRenderState(FPrimitiveRenderState* InOutRenderState);
    virtual void DestroyRenderState(FPrimitiveRenderState* InRenderState);
    
    static const UClass& StaticClass();
    
private:
    bool bIsVisible_ = true;
    float CullDistance_ = 0.0f;  // 0 = 始终渲染
    FBounds CachedBounds_;
};
```

**关键设计**：

- **可见性**：简单的 `bIsVisible_` 开关，为后续遮挡、LOD、视锥剔除预留接口。
- **Bounds**：返回 AABB 和包围球，用于剔除计算。当前可返回默认值，后续由子类（MeshComponent）精确计算。
- **渲染代理接口**：`CreateRenderState` / `UpdateRenderState` / `DestroyRenderState` 为后续 SceneProxy 扩展预留，当前可实现为直接创建 D3D12 资源或持有渲染数据。

### 3.5 UMeshComponent

**文件**：`src/components/MeshComponent.h`、`src/components/MeshComponent.cpp`

**职责**：绑定网格资源的组件基类，引用 Asset 并提供材质覆盖、LOD 接口。

```cpp
class UMeshComponent : public UPrimitiveComponent
{
public:
    UMeshComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass);
    virtual ~UMeshComponent() = default;
    
    // --- Mesh 资源引用 ---
    // 使用「data 相对路径 + Name」标识资源
    void SetMeshAssetId(const std::string& InAssetId);  // 例如 "meshes/building/house"
    const std::string& GetMeshAssetId() const;
    
    // 设置/获取渲染资源对象（运行时解析）
    void SetMeshAsset(UStreamableRenderAsset* InAsset);
    UStreamableRenderAsset* GetMeshAsset() const;
    
    // --- 材质覆盖 ---
    struct FMaterialOverride
    {
        int32 MaterialSlot = 0;  // Mesh 的材质槽位
        std::string MaterialAssetId;  // 覆盖的材质资源 ID
    };
    
    void SetMaterialOverride(int32 InSlot, const std::string& InMaterialAssetId);
    void ClearMaterialOverride(int32 InSlot);
    const std::vector<FMaterialOverride>& GetMaterialOverrides() const;
    
    // --- LOD（预留接口） ---
    void SetForcedLODLevel(int32 InLODLevel);
    int32 GetForcedLODLevel() const;
    virtual int32 GetCurrentLODLevel() const;  // 基于距离计算
    
    // --- Bounds 重写 ---
    virtual FBounds GetBounds() const override;
    
    static const UClass& StaticClass();
    
private:
    std::string MeshAssetId_;  // 资源标识（路径+Name）
    UStreamableRenderAsset* MeshAsset_ = nullptr;  // 运行时解析的资源对象
    
    std::vector<FMaterialOverride> MaterialOverrides_;
    int32 ForcedLODLevel_ = 0;  // 0 = 自动
};
```

**关键设计**：

- **Asset 引用**：通过 `MeshAssetId_`（字符串）引用资源，运行时通过资源注册表解析为 `UStreamableRenderAsset*`。
- **材质覆盖**：每个槽位可单独覆盖材质，与 Asset 的默认材质组合使用。
- **LOD**：预留 `ForcedLODLevel` 与计算接口，当前可仅返回 0（最高 LOD）。

### 3.6 UStaticMeshComponent

**文件**：`src/components/StaticMeshComponent.h`、`src/components/StaticMeshComponent.cpp`

**职责**：渲染不可变形静态网格。

```cpp
class UStaticMeshComponent : public UMeshComponent
{
public:
    UStaticMeshComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass);
    virtual ~UStaticMeshComponent() = default;
    
    // --- 渲染状态更新 ---
    virtual void CreateRenderState(FPrimitiveRenderState* OutRenderState) override;
    virtual void UpdateRenderState(FPrimitiveRenderState* InOutRenderState) override;
    virtual void DestroyRenderState(FPrimitiveRenderState* InRenderState) override;
    
    static const UClass& StaticClass();
};
```

**关键设计**：

- 继承 `UMeshComponent` 的资源引用能力。
- 实现渲染状态创建/更新：从 `UStaticMesh` Asset 获取顶点/索引缓冲，结合本 Component 的世界变换与材质覆盖，生成绘制描述。
- 当前可直接调用 `Dx12Renderer`，后续迁移到 SceneProxy。

### 3.7 USkinnedMeshComponent

**文件**：`src/components/SkinnedMeshComponent.h`、`src/components/SkinnedMeshComponent.cpp`

**职责**：支持骨骼驱动的蒙皮网格基类。

```cpp
class USkinnedMeshComponent : public UMeshComponent
{
public:
    USkinnedMeshComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass);
    virtual ~USkinnedMeshComponent() = default;
    
    // --- 骨骼姿态 ---
    struct FBoneTransform
    {
        int32 BoneIndex;
        FTransform Transform;  // 相对于参考姿态的变换
    };
    
    // 设置骨骼矩阵数组（由动画系统驱动）
    void SetBoneTransforms(const std::vector<FBoneTransform>& InBoneTransforms);
    const std::vector<FBoneTransform>& GetBoneTransforms() const;
    
    // --- 渲染状态更新 ---
    virtual void CreateRenderState(FPrimitiveRenderState* OutRenderState) override;
    virtual void UpdateRenderState(FPrimitiveRenderState* InOutRenderState) override;
    
    static const UClass& StaticClass();
    
private:
    std::vector<FBoneTransform> BoneTransforms_;  // 当前帧骨骼变换
};
```

**关键设计**：

- **骨骼数据**：不存储骨骼拓扑（由 Asset 持有），只存储当前帧的骨骼变换数组。
- **动画驱动**：由外部动画系统（如动画蓝图、混合空间）每帧调用 `SetBoneTransforms()` 更新姿态。

### 3.8 USkeletalMeshComponent

**文件**：`src/components/SkeletalMeshComponent.h`、`src/components/SkeletalMeshComponent.cpp`

**职责**：骨骼网格的具象实现。

```cpp
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
    USkeletalMeshComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass);
    virtual ~USkeletalMeshComponent() = default;
    
    // 渲染状态实现（蒙皮顶点处理）
    virtual void CreateRenderState(FPrimitiveRenderState* OutRenderState) override;
    virtual void UpdateRenderState(FPrimitiveRenderState* InOutRenderState) override;
    
    static const UClass& StaticClass();
};
```

**关键设计**：

- 继承 `USkinnedMeshComponent`，引用 `USkeletalMesh` Asset。
- 实现蒙皮渲染：在 GPU 蒙皮模式下，将骨骼矩阵传入 Shader；在 CPU 蒙皮模式下，先在 CPU 计算蒙皮顶点再提交。

---

## 4. Asset 继承体系

### 4.1 类层次

```
UObject
  └── UStreamableRenderAsset (src/render/asset/StreamableRenderAsset.h/.cpp)
        ├── UStaticMesh (src/render/asset/StaticMesh.h/.cpp)
        └── USkinnedAsset (src/render/asset/SkinnedAsset.h/.cpp)
              └── USkeletalMesh (src/render/asset/SkeletalMesh.h/.cpp)
```

### 4.2 UStreamableRenderAsset

**文件**：`src/render/asset/StreamableRenderAsset.h`、`src/render/asset/StreamableRenderAsset.cpp`

**职责**：可渲染资源的基类，提供资源标识、流式加载预留接口。

```cpp
class UStreamableRenderAsset : public UObject
{
public:
    UStreamableRenderAsset(uint64_t InObjectId, std::string InObjectName, const UClass* InClass);
    virtual ~UStreamableRenderAsset() = default;
    
    // --- 资源标识 ---
    void SetAssetPath(const std::filesystem::path& InRelativePath);
    const std::filesystem::path& GetAssetPath() const;  // 相对 data 的路径
    
    // --- 流式加载预留接口（当前不实现） ---
    enum class ELoadingStatus
    {
        Unloaded,
        Loading,
        Loaded,
        Failed
    };
    
    ELoadingStatus GetLoadingStatus() const;
    virtual void RequestLoad();  // 请求加载（预留）
    virtual void Release();      // 释放资源（预留）
    
    // --- 序列化 ---
    virtual void Serialize(nlohmann::json* OutObjectJson) const override;
    
    static const UClass& StaticClass();
    
private:
    std::filesystem::path AssetPath_;  // 相对 data 目录的路径
    ELoadingStatus LoadingStatus_ = ELoadingStatus::Unloaded;
};
```

**关键设计**：

- **AssetPath**：存储「data 相对路径」，与「路径 + Name」标识对应。
- **流式加载预留**：`RequestLoad()` / `Release()` 为空实现，当前始终处于 Loaded 状态，后续可实现分块加载。

### 4.3 UStaticMesh

**文件**：`src/render/asset/StaticMesh.h`、`src/render/asset/StaticMesh.cpp`

**职责**：静态网格的纯数据容器。

```cpp
class UStaticMesh : public UStreamableRenderAsset
{
public:
    // --- 顶点格式 ---
    struct FVertex
    {
        FVector3 Position;
        FVector3 Normal;
        FVector2D TexCoord;
        FVector4 Tangent;  // XYZ = T, W = Binormal sign
    };
    
    // --- 子网格（Section） ---
    struct FStaticMeshSection
    {
        uint32_t MaterialIndex = 0;  // 关联材质槽位
        uint32_t FirstIndex = 0;
        uint32_t IndexCount = 0;
        FBounds SectionBounds;
        
        // 顶点/索引缓冲（GPU 资源句柄或 D3D12 资源指针）
        // 当前：直接存储 GPU 资源指针；后续可抽象为 RHI 资源句柄
        void* VertexBufferGPU = nullptr;
        void* IndexBufferGPU = nullptr;
    };
    
    UStaticMesh(uint64_t InObjectId, std::string InObjectName, const UClass* InClass);
    virtual ~UStaticMesh() = default;
    
    // --- 数据操作 ---
    void SetVertices(const std::vector<FVertex>& InVertices);
    const std::vector<FVertex>& GetVertices() const;
    
    void SetIndices(const std::vector<uint32_t>& InIndices);
    const std::vector<uint32_t>& GetIndices() const;
    
    // --- Section 操作 ---
    void AddSection(const FStaticMeshSection& InSection);
    size_t GetSectionCount() const;
    const FStaticMeshSection& GetSection(size_t InIndex) const;
    
    // --- LOD ---
    void SetLODCount(size_t InLODCount);
    size_t GetLODCount() const;
    // 简化：每个 LOD 一组分 Section，后续可扩展
    
    // --- Bounds ---
    FBounds GetBounds() const;
    
    static const UClass& StaticClass();
    
private:
    std::vector<FVertex> Vertices_;
    std::vector<uint32_t> Indices_;
    std::vector<FStaticMeshSection> Sections_;  // 默认 LOD0
    FBounds TotalBounds_;
};
```

**关键设计**：

- **Section**：支持多材质槽位，每个 Section 可独立设置材质。
- **GPU 资源句柄**：当前可用 `void*` 或 D3D12 资源指针，后续抽象为 RHI 资源接口。
- **LOD**：预留 `Sections_` 数组支持多级 LOD，当前可仅实现 LOD0。

### 4.4 USkinnedAsset

**文件**：`src/render/asset/SkinnedAsset.h`、`src/render/asset/SkinnedAsset.cpp`

**职责**：蒙皮资源基类，存储骨骼层级与蒙皮数据。

```cpp
class USkinnedAsset : public UStreamableRenderAsset
{
public:
    // --- 骨骼信息 ---
    struct FBone
    {
        std::string Name;
        int32 ParentIndex = -1;  // -1 = 根骨骼
        FTransform ReferencePose;  // 参考姿态（绑定姿势）
    };
    
    // --- 顶点蒙皮权重 ---
    struct FSkinVertex
    {
        FVector3 Position;
        FVector3 Normal;
        FVector2D TexCoord;
        FVector4 Tangent;
        
        // 蒙皮权重：最多 4 骨骼
        std::array<int32, 4> BoneIndices{};
        std::array<float, 4> BoneWeights{};
    };
    
    USkinnedAsset(uint64_t InObjectId, std::string InObjectName, const UClass* InClass);
    virtual ~USkinnedAsset() = default;
    
    // --- 骨骼操作 ---
    void SetSkeleton(const std::vector<FBone>& InBones);
    const std::vector<FBone>& GetSkeleton() const;
    int32 FindBoneIndex(const std::string& InBoneName) const;
    
    // --- 蒙皮数据 ---
    void SetSkinVertices(const std::vector<FSkinVertex>& InVertices);
    const std::vector<FSkinVertex>& GetSkinVertices() const;
    
    static const UClass& StaticClass();
    
private:
    std::vector<FBone> Skeleton_;  // 骨骼层级
    std::vector<FSkinVertex> SkinVertices_;  // 带蒙皮信息的顶点
};
```

**关键设计**：

- **骨骼层级**：`Skeleton_` 数组，`ParentIndex` 构成骨骼树结构。
- **蒙皮权重**：每个顶点最多受 4 个骨骼影响，标准 GPU 蒙皮做法。

### 4.5 USkeletalMesh

**文件**：`src/render/asset/SkeletalMesh.h`、`src/render/asset/SkeletalMesh.cpp`

**职责**：骨骼网格的完整数据资产。

```cpp
class USkeletalMesh : public USkinnedAsset
{
public:
    // --- 带索引的 Section ---
    struct FSkeletalMeshSection
    {
        uint32_t MaterialIndex = 0;
        uint32_t FirstIndex = 0;
        uint32_t IndexCount = 0;
        
        // 骨骼索引列表（该 Section 影响的骨骼子集）
        std::vector<int32_t> BoneIndices;
        
        // GPU 资源
        void* VertexBufferGPU = nullptr;
        void* IndexBufferGPU = nullptr;
    };
    
    USkeletalMesh(uint64_t InObjectId, std::string InObjectName, const UClass* InClass);
    virtual ~USkeletalMesh() = default;
    
    // --- Section 操作 ---
    void AddSection(const FSkeletalMeshSection& InSection);
    size_t GetSectionCount() const;
    const FSkeletalMeshSection& GetSection(size_t InIndex) const;
    
    // --- LOD（预留） ---
    void SetLODCount(size_t InLODCount);
    size_t GetLODCount() const;
    
    static const UClass& StaticClass();
    
private:
    std::vector<FSkeletalMeshSection> Sections_;  // 默认 LOD0
};
```

**关键设计**：

- 继承 `USkinnedAsset` 的骨骼与蒙皮数据能力。
- **Section**：每个 Section 记录其影响的骨骼子集（BoneIndices），便于 GPU 蒙皮 Shader 只传入必要骨骼矩阵。
- **LOD**：预留接口，当前仅支持 LOD0。

---

## 5. AActor 扩展：RootComponent

### 5.1 扩展内容

为 `AActor` 添加根组件支持，迁移现有的 `Transform` 到 `RootComponent`：

```cpp
class AActor : public UObject
{
public:
    // ... 现有成员 ...
    
    // --- 根组件 ---
    void SetRootComponent(USceneComponent* InRootComponent);
    USceneComponent* GetRootComponent() const;
    
    // --- 组件遍历 ---
    template<typename T>
    T* FindComponentByClass() const;
    
    void GetComponents(std::vector<UActorComponent*>& OutComponents) const;
    void AddComponent(UActorComponent* InComponent);
    void RemoveComponent(UActorComponent* InComponent);
    
    // --- 变换兼容（保留用于迁移期） ---
    // 迁移完成后可移除或标记废弃
    const FVector3& GetActorLocation() const;
    void SetActorLocation(const FVector3& InLocation);
    
    virtual void Tick(float InDeltaSeconds) override;
    
private:
    USceneComponent* RootComponent_ = nullptr;
    std::vector<UActorComponent*> Components_;  // 所有组件（包括 RootComponent）
};
```

### 5.2 变换迁移策略

1. **初始化时**：创建默认 `USceneComponent` 作为 `RootComponent_`，将原 `Transform_` 数据复制过去。
2. **访问时**：`GetTransform()` 调用 `RootComponent_->GetWorldTransform()`。
3. **兼容性**：保留 `AActor::Transform` 的 getter/setter，内部转发到 `RootComponent`。

---

## 6. 资源引用与加载

### 6.1 资源注册表（ResourceRegistry）

```cpp
class ResourceRegistry
{
public:
    // 注册 / 注销 Asset
    void RegisterAsset(UStreamableRenderAsset* InAsset);
    void UnregisterAsset(const std::string& InAssetId);
    
    // 按 ID 查找
    UStreamableRenderAsset* FindAsset(const std::string& InAssetId);
    const UStreamableRenderAsset* FindAsset(const std::string& InAssetId) const;
    
    // 按类型查找（模板）
    template<typename T>
    T* FindAsset(const std::string& InAssetId);
    
private:
    std::unordered_map<std::string, UStreamableRenderAsset*> Assets_;
};
```

**资源 ID 格式**：`meshes/building/house`（相对 data 目录，不含扩展名或含扩展名需约定）。

### 6.2 Component 资源解析

```cpp
void UMeshComponent::SetMeshAsset(UStreamableRenderAsset* InAsset)
{
    MeshAsset_ = InAsset;
    // 触发渲染状态重建
    MarkRenderStateDirty();
}
```

或在 `Initialize()` 时解析 ID：

```cpp
void UMeshComponent::Initialize()
{
    if (!MeshAssetId_.empty() && MeshAsset_ == nullptr)
    {
        MeshAsset_ = ResourceRegistry::Get().FindAsset<USkeletalMesh>(MeshAssetId_);
    }
}
```

---

## 7. 渲染提交流程（当前实现）

### 7.1 主循环中的渲染收集

```cpp
// GameApp::Tick() 或 World::Tick()
void UWorld::Render(IRendererInterface* InRenderer)
{
    // 1. 遍历所有 Level
    for (ULevel* Level : LoadedLevels_)
    {
        // 2. 遍历 Level 所有 Actor
        for (AActor* Actor : Level->GetActors())
        {
            // 3. 遍历 Actor 所有 PrimitiveComponent
            for (UActorComponent* Comp : Actor->GetComponents())
            {
                if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Comp))
                {
                    if (Primitive->IsVisible())
                    {
                        // 4. 收集可见 Primitive 的渲染描述
                        RenderCollector.AddPrimitive(Primitive);
                    }
                }
            }
        }
    }
    
    // 5. 提交给渲染器
    InRenderer->Draw(RenderCollector.BuildRenderCommands());
}
```

### 7.2 渲染描述构建

```cpp
struct FMeshDrawCommand
{
    UStreamableRenderAsset* MeshAsset;
    FTransform WorldTransform;
    std::vector<FMaterialOverride> MaterialOverrides;
    // ... 其他渲染参数
};

class FRenderCollector
{
public:
    void AddPrimitive(UPrimitiveComponent* InPrimitive);
    std::vector<FMeshDrawCommand> BuildRenderCommands() const;
    
private:
    std::vector<UPrimitiveComponent*> VisiblePrimitives_;
};
```

---

## 8. 序列化

### 8.1 Actor + Component 序列化

```json
{
  "object_id": 12345,
  "object_name": "MyActor",
  "class": "AActor",
  "components": [
    {
      "object_id": 12346,
      "object_name": "RootComponent",
      "class": "USceneComponent",
      "relative_transform": { "location": [0, 0, 0], "rotation": [0, 0, 0], "scale": [1, 1, 1] }
    },
    {
      "object_id": 12347,
      "object_name": "StaticMesh",
      "class": "UStaticMeshComponent",
      "mesh_asset_id": "meshes/building/house",
      "material_overrides": [
        { "slot": 0, "material_id": "materials/stone/wall" }
      ],
      "attach_parent": 12346
    }
  ]
}
```

### 8.2 Asset 序列化

```json
{
  "object_id": 20001,
  "object_name": "house",
  "class": "UStaticMesh",
  "asset_path": "meshes/building/house",
  "vertices": [ ... ],
  "indices": [ ... ],
  "sections": [
    { "material_index": 0, "first_index": 0, "index_count": 1500 }
  ]
}
```

---

## 9. Assimp 导入衔接

### 9.1 导入流程

1. **解析**：使用 Assimp 读取 `.fbx` / `.obj` / `.gltf` 文件。
2. **转换**：
   - 顶点 → `UStaticMesh::FVertex` 或 `USkeletalMesh::FSkinVertex`
   - 索引 → `UStaticMesh::Indices_`
   - 材质 → 生成 `UMaterial`（如需要）
3. **创建 Asset**：
   ```cpp
   UStaticMesh* Mesh = Registry.NewObject<UStaticMesh>(...);
   Mesh->SetAssetPath("meshes/character/knight");
   Mesh->SetVertices(ConvertedVertices);
   Mesh->SetIndices(ConvertedIndices);
   // 为每个 Material 创建 Section
   ```
4. **建立引用树**：
   - Mesh 作为 Outer，Material 作为 Inner
5. **持久化**：写入项目 data 目录（如 `data/meshes/character/knight.json`）。

### 9.2 资源管理器扩展

扩展 `ResourceLoader`：

```cpp
class ResourceLoader
{
public:
    // 加载/查找 UStaticMesh
    UStaticMesh* LoadStaticMesh(const std::filesystem::path& InRelativePath);
    
    // 加载/查找 USkeletalMesh
    USkeletalMesh* LoadSkeletalMesh(const std::filesystem::path& InRelativePath);
    
    // 注册到全局 ResourceRegistry
    void RegisterLoadedAsset(UStreamableRenderAsset* InAsset);
};
```

---

## 10. 文件结构

```
src/
  core/
    UObject.h / .cpp        # 已扩展 Outer/Inner
    UClass.h / .cpp
    
  world/
    Actor.h / .cpp          # 已扩展 RootComponent
    
  components/
    ActorComponent.h / .cpp
    SceneComponent.h / .cpp
    PrimitiveComponent.h / .cpp
    MeshComponent.h / .cpp
    StaticMeshComponent.h / .cpp
    SkinnedMeshComponent.h / .cpp
    SkeletalMeshComponent.h / .cpp
    
  render/
    asset/
      StreamableRenderAsset.h / .cpp
      StaticMesh.h / .cpp
      SkinnedAsset.h / .cpp
      SkeletalMesh.h / .cpp
      
    ResourceRegistry.h / .cpp  # 新增：资源注册表
    RenderCollector.h / .cpp   # 新增：渲染收集器
    
  data/
    ResourceLoader.h / .cpp    # 已扩展资产加载
```

---

## 11. 后续扩展点

| 扩展项 | 当前状态 | 后续实现 |
|--------|----------|----------|
| **SceneProxy** | `CreateRenderState` 直接创建 D3D12 资源 | Component 创建轻量 Proxy，RenderThread 只读 Proxy 构建 DrawCall |
| **RenderThread** | GameThread 直接提交渲染 | Component 在 RenderThread 构建/更新 SceneProxy，RenderThread 生成 DrawCall |
| **RHI 抽象** | 直接使用 D3D12 资源句柄 | 抽象 `IRHIResource`、`IRHICommandList` 接口，支持 Vulkan/Metal |
| **流式加载** | `UStreamableRenderAsset` 预留接口 | 实现 `RequestLoad` / `Release`，按需分块加载顶点/纹理 |
| **动画系统** | `USkinnedMeshComponent` 预留骨骼变换接口 | 引入 `UAnimBlueprint`、`UAnimInstance`，每帧驱动 `SetBoneTransforms` |
| **LOD** | 预留 `LODCount` / `ForcedLODLevel` | 基于距离自动切换 LOD，支持按屏幕尺寸/质量设置 |
| **遮挡剔除** | 预留 `GetBounds` | 实现 Hierarchical Z-Buffer 或 CPU 端视锥/遮挡剔除 |

---

## 12. 验收标准

1. **Component 创建**：可在 Actor 上成功添加 UStaticMeshComponent，并设置 MeshAssetId。
2. **空间层级**：SceneComponent 的父子 Attach 与世界变换计算正确。
3. **Asset 加载**：通过 ResourceLoader 加载 .fbx 并转换为 UStaticMesh，资源 ID 正确。
4. **渲染提交**：可见的 UStaticMeshComponent 能正确绘制到屏幕。
5. **序列化**：Actor + Component + Asset 引用关系能序列化为 JSON 并从 JSON 恢复。
6. **兼容性**：现有 World/Level/Actor 生命周期不受影响，现有渲染路径可fallback。

---

**设计状态**：完成  
**最后更新**：2026-03-12
