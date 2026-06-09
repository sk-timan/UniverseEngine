#pragma once

#include <string>

#include "asset/AssetRegistry.h"

class FAssetDuplicateService
{
public:
	static bool DuplicateAsset(
		const FAssetRegistryEntry& InSourceEntry,
		const std::string& InTargetFolderPath,
		std::string* OutNewSoftObjectPath,
		std::string* OutErrorMessage);

	static std::string BuildAssetPathInFolder(const std::string& InFolderPath, const std::string& InObjectName);
	static std::string FindUniqueObjectName(
		const std::string& InFolderPath,
		const std::string& InBaseName,
		const std::string& InExcludeAssetPath = "");

private:
	static bool AssetObjectNameExistsInFolder(
		const std::string& InFolderPath,
		const std::string& InObjectName,
		const std::string& InExcludeAssetPath);
};
