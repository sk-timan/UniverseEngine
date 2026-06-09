#pragma once

#include <string>

#include "asset/AssetRegistry.h"

class FAssetRenameService
{
public:
	static bool RenameAssetObject(
		const FAssetRegistryEntry& InEntry,
		const std::string& InNewObjectName,
		std::string* OutErrorMessage);

	static bool RenameFolder(
		const std::string& InFolderPath,
		const std::string& InNewFolderName,
		std::string* OutErrorMessage);

	static bool CreateFolder(
		const std::string& InParentFolderPath,
		const std::string& InNewFolderName,
		std::string* OutNewFolderPath,
		std::string* OutErrorMessage);

	static bool MoveAssetToFolder(
		const FAssetRegistryEntry& InEntry,
		const std::string& InTargetFolderPath,
		std::string* OutNewSoftObjectPath,
		std::string* OutErrorMessage);

	static bool MoveFolder(
		const std::string& InSourceFolderPath,
		const std::string& InTargetFolderPath,
		std::string* OutErrorMessage);

private:
	static bool IsValidRenameToken(const std::string& InName);
};
