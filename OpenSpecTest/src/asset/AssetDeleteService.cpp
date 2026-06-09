#include "asset/AssetDeleteService.h"

#include <filesystem>

#include "asset/AssetFolderTree.h"
#include "asset/AssetManager.h"
#include "asset/AssetRegistry.h"
#include "asset/AssetSerializer.h"
#include "asset/ProjectPaths.h"
#include "asset/SoftObjectPath.h"
#include "asset/thumbnail/AssetThumbnailService.h"
#include "render/ResourceRegistry.h"

namespace
{
bool IsProtectedFolderPath(const std::string& InFolderPath)
{
	return InFolderPath.empty() || InFolderPath == "All" || InFolderPath == "Content";
}

std::filesystem::path ResolveFolderDiskPath(const std::string& InFolderPath)
{
	if (InFolderPath == "Content")
	{
		return GProjectContentDirectory;
	}

	return ResolveContentFilePath(Utf8GenericToFsPath(InFolderPath));
}
} // namespace

bool FAssetDeleteService::DeleteAsset(const FAssetRegistryEntry& InEntry, std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}

	if (InEntry.AssetPath.empty())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Asset path is empty.";
		}
		return false;
	}

	const std::string SoftPath = FSoftObjectPath::Build(InEntry.AssetPath, InEntry.ObjectName);
	UAssetManager::Get().Unload(SoftPath);
	ResourceRegistry::Get().UnregisterAssetByPath(std::filesystem::path(SoftPath));

	std::error_code ErrorCode;
	if (!InEntry.UAssetFilePath.empty() && std::filesystem::exists(InEntry.UAssetFilePath))
	{
		if (!std::filesystem::remove(InEntry.UAssetFilePath, ErrorCode))
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Failed to delete uasset file: " + InEntry.UAssetFilePath.string();
				if (ErrorCode)
				{
					*OutErrorMessage += " (" + ErrorCode.message() + ")";
				}
			}
			return false;
		}
	}

	const std::filesystem::path MetaPath = UAssetSerializer::GetMetaPathForUAsset(InEntry.UAssetFilePath);
	if (!MetaPath.empty() && std::filesystem::exists(MetaPath))
	{
		std::filesystem::remove(MetaPath, ErrorCode);
	}

	FAssetThumbnailService::Get().InvalidateEntry(InEntry);
	FAssetRegistry::Get().RemoveByAssetPath(InEntry.AssetPath);
	return true;
}

size_t FAssetDeleteService::CountAssetsInFolder(
	const std::vector<FAssetRegistryEntry>& InAllEntries,
	const std::string& InFolderPath)
{
	if (IsProtectedFolderPath(InFolderPath))
	{
		return 0;
	}

	size_t Count = 0;
	for (const FAssetRegistryEntry& Entry : InAllEntries)
	{
		if (AssetFolderTreeBuilder::IsAssetInFolder(Entry, InFolderPath))
		{
			++Count;
		}
	}

	return Count;
}

bool FAssetDeleteService::DeleteFolder(
	const std::vector<FAssetRegistryEntry>& InAllEntries,
	const std::string& InFolderPath,
	std::vector<std::string>* OutDeletedSoftPaths,
	std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}

	if (IsProtectedFolderPath(InFolderPath))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Cannot delete protected folder.";
		}
		return false;
	}

	std::vector<FAssetRegistryEntry> AssetsToDelete;
	for (const FAssetRegistryEntry& Entry : InAllEntries)
	{
		if (AssetFolderTreeBuilder::IsAssetInFolder(Entry, InFolderPath))
		{
			AssetsToDelete.push_back(Entry);
		}
	}

	for (const FAssetRegistryEntry& Entry : AssetsToDelete)
	{
		const std::string SoftPath = FSoftObjectPath::Build(Entry.AssetPath, Entry.ObjectName);
		std::string AssetErrorMessage;
		if (!DeleteAsset(Entry, &AssetErrorMessage))
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = AssetErrorMessage;
			}
			return false;
		}

		if (OutDeletedSoftPaths != nullptr)
		{
			OutDeletedSoftPaths->push_back(SoftPath);
		}
	}

	const std::filesystem::path DiskPath = ResolveFolderDiskPath(InFolderPath);
	std::error_code ErrorCode;
	if (!DiskPath.empty() && std::filesystem::exists(DiskPath))
	{
		if (!std::filesystem::remove_all(DiskPath, ErrorCode))
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Failed to delete folder from disk: " + DiskPath.string();
				if (ErrorCode)
				{
					*OutErrorMessage += " (" + ErrorCode.message() + ")";
				}
			}
			return false;
		}
	}

	return true;
}
