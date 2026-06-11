## ADDED Requirements

### 需求:Registry 必须索引 Texture2D 类型

AssetRegistry 扫描与 ListAssets 过滤必须支持 `Texture2D` 类型条目。

#### 场景:按类型列出纹理

- **当** 调用 ListAssets 并过滤 type=`Texture2D`
- **那么** 系统必须仅返回 Texture2D 类型 uasset 的路径列表

#### 场景:Texture 条目 Source 信息

- **当** Registry 索引 Texture2D uasset
- **那么** 条目必须可读 meta.source_file 供 Reimport 与 Content Browser 展示

#### 场景:depends_on 预留 Material 引用

- **当** Texture uasset header 含 depends_on
- **那么** Registry 必须原样暴露 depends_on 列表（可为空）
