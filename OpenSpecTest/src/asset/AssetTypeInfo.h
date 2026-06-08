#pragma once

#include <string>

#include <QColor>

struct FAssetTypeDisplayInfo
{
	std::string DisplayName;
	QColor AccentColor;
};

class AssetTypeInfo
{
public:
	static FAssetTypeDisplayInfo GetDisplayInfo(const std::string& InType);
	static bool IsRenderableAssetType(const std::string& InType);
	static bool IsMeshAssetType(const std::string& InType);
};
