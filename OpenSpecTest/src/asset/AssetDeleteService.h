#pragma once

#include <string>
#include <vector>

#include "asset/AssetRegistry.h"

class FAssetDeleteService
{
public:
	static bool DeleteAsset(const FAssetRegistryEntry& InEntry, std::string* OutErrorMessage);
	static size_t CountAssetsInFolder(
		const std::vector<FAssetRegistryEntry>& InAllEntries,
		const std::string& InFolderPath);
	static bool DeleteFolder(
		const std::vector<FAssetRegistryEntry>& InAllEntries,
		const std::string& InFolderPath,
		std::vector<std::string>* OutDeletedSoftPaths,
		std::string* OutErrorMessage);
};
