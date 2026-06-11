## ADDED Requirements

### 需求:AssetManager Load 必须支持 Texture2D 类型路由

`UAssetSerializer::LoadObject` 必须根据 header.type 反序列化 `UTexture2D`，并纳入 AssetManager 缓存。

#### 场景:按 type 反序列化 Texture2D

- **当** uasset header.type 为 `Texture2D`
- **那么** LoadObject 必须返回 `UTexture2D` 实例
- **那么** 禁止以 StaticMesh 或 JSON fallback 解析 Texture payload
