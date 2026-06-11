## ADDED Requirements

### 需求:Import 规范必须涵盖 Texture2D 类型

资产 Import 规范必须将 Texture 与 Mesh 并列，均要求 uasset + meta 持久化与 Factory 统一入口。

#### 场景:Content Browser 导入纹理

- **当** 用户在 Content Browser 选择导入图片并指定 Content 目标路径
- **那么** 系统必须调用 `UTextureImportFactory` 而非 `UMeshImportFactory`
- **那么** 生成的 uasset header.type 必须为 `Texture2D`

#### 场景:Import 后 Registry 立即可见

- **当** Texture Import Factory 成功写入 uasset
- **那么** AssetRegistry 必须立即索引该 Texture2D 条目，无需重启 Editor
