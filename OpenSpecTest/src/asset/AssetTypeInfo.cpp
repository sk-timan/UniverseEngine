#include "asset/AssetTypeInfo.h"

FAssetTypeDisplayInfo AssetTypeInfo::GetDisplayInfo(const std::string& InType)
{
	if (InType == "StaticMesh")
	{
		return {"Static Mesh", QColor("#5bc0de")};
	}
	if (InType == "SkeletalMesh")
	{
		return {"Skeletal Mesh", QColor("#f0ad4e")};
	}
	if (InType == "Texture2D")
	{
		return {"Texture 2D", QColor("#5cb85c")};
	}
	if (InType == "Texture")
	{
		return {"Texture", QColor("#5cb85c")};
	}
	if (InType == "Material")
	{
		return {"Material", QColor("#9b59b6")};
	}

	const std::string DisplayName = InType.empty() ? "Unknown" : InType;
	return {DisplayName, QColor("#808080")};
}

bool AssetTypeInfo::IsRenderableAssetType(const std::string& InType)
{
	return InType == "StaticMesh" || InType == "SkeletalMesh" || InType == "Texture2D" || InType == "Texture";
}

bool AssetTypeInfo::IsTextureAssetType(const std::string& InType)
{
	return InType == "Texture2D" || InType == "Texture";
}

bool AssetTypeInfo::IsMeshAssetType(const std::string& InType)
{
	return InType == "StaticMesh" || InType == "SkeletalMesh";
}
