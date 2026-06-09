#pragma once

#include <memory>
#include <string>
#include <vector>

#include "asset/AssetRegistry.h"

struct FAssetFolderNode
{
	std::string Path;
	std::string DisplayName;
	std::vector<std::unique_ptr<FAssetFolderNode>> Children;
};

class AssetFolderTreeBuilder
{
public:
	static FAssetFolderNode BuildFromRegistry(const std::vector<FAssetRegistryEntry>& InEntries);
	static bool IsAssetInFolder(const FAssetRegistryEntry& InEntry, const std::string& InFolderPath);
	static bool IsAssetDirectChildOfFolder(const FAssetRegistryEntry& InEntry, const std::string& InFolderPath);
};
