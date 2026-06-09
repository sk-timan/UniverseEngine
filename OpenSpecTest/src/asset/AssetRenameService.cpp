#include "asset/AssetRenameService.h"

#include <algorithm>
#include <filesystem>

#include "asset/AssetDuplicateService.h"
#include "asset/AssetRedirectStore.h"
#include "asset/AssetRegistry.h"
#include "asset/AssetSerializer.h"
#include "asset/ProjectPaths.h"
#include "asset/SoftObjectPath.h"
#include "asset/thumbnail/AssetThumbnailService.h"
#include "render/ResourceRegistry.h"
#include "render/asset/StreamableRenderAsset.h"

namespace
{
bool StartsWithPathPrefix(const std::string& InPath, const std::string& InPrefix)
{
	if (InPath == InPrefix)
	{
		return true;
	}
	if (InPath.size() <= InPrefix.size())
	{
		return false;
	}
	return InPath.compare(0, InPrefix.size() + 1, InPrefix + "/") == 0;
}

std::string ReplacePathPrefix(const std::string& InPath, const std::string& InOldPrefix, const std::string& InNewPrefix)
{
	if (InPath == InOldPrefix)
	{
		return InNewPrefix;
	}
	if (!StartsWithPathPrefix(InPath, InOldPrefix))
	{
		return InPath;
	}
	return InNewPrefix + InPath.substr(InOldPrefix.size());
}

bool RelocateAssetPath(
	const FAssetRegistryEntry& InEntry,
	const std::string& InNewAssetPath,
	std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}

	if (InNewAssetPath.empty())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Target asset path is empty.";
		}
		return false;
	}

	if (InNewAssetPath == InEntry.AssetPath)
	{
		return true;
	}

	FAssetPackageHeader Header;
	if (!UAssetSerializer::LoadHeader(InEntry.UAssetFilePath, &Header, OutErrorMessage))
	{
		return false;
	}

	std::filesystem::path NewRelativeUAssetPath(InNewAssetPath);
	NewRelativeUAssetPath.replace_extension(".uasset");
	const std::filesystem::path NewUAssetPath = ResolveContentFilePath(NewRelativeUAssetPath);
	if (NewUAssetPath != InEntry.UAssetFilePath && std::filesystem::exists(NewUAssetPath))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Target asset already exists: " + NewUAssetPath.string();
		}
		return false;
	}

	UStreamableRenderAsset* LoadedAsset = UAssetSerializer::LoadObject(InEntry.UAssetFilePath, OutErrorMessage);
	if (LoadedAsset == nullptr)
	{
		return false;
	}

	const std::string OldSoftPath = FSoftObjectPath::Build(Header.AssetPath, Header.ObjectName);
	ResourceRegistry::Get().UnregisterAssetByPath(std::filesystem::path(OldSoftPath));

	Header.AssetPath = InNewAssetPath;
	LoadedAsset->SetAssetPath(InNewAssetPath);

	if (!UAssetSerializer::Save(Header, *LoadedAsset, NewUAssetPath, OutErrorMessage))
	{
		delete LoadedAsset;
		return false;
	}

	const std::filesystem::path OldMetaPath = UAssetSerializer::GetMetaPathForUAsset(InEntry.UAssetFilePath);
	const std::filesystem::path NewMetaPath = UAssetSerializer::GetMetaPathForUAsset(NewUAssetPath);
	std::error_code ErrorCode;
	if (std::filesystem::exists(OldMetaPath))
	{
		std::filesystem::create_directories(NewMetaPath.parent_path(), ErrorCode);
		std::filesystem::rename(OldMetaPath, NewMetaPath, ErrorCode);
	}

	if (NewUAssetPath != InEntry.UAssetFilePath)
	{
		std::filesystem::remove(InEntry.UAssetFilePath, ErrorCode);
	}

	delete LoadedAsset;

	FAssetRegistry::Get().RemoveByAssetPath(InEntry.AssetPath);
	FAssetRegistry::Get().RegisterFromHeader(Header, NewUAssetPath);
	FAssetThumbnailService::Get().InvalidateEntry(InEntry);
	const std::string NewSoftPath = FSoftObjectPath::Build(Header.AssetPath, Header.ObjectName);
	FAssetRedirectStore::Get().RecordRedirect(OldSoftPath, NewSoftPath);
	return true;
}

std::filesystem::path ResolveFolderDiskPath(const std::string& InFolderPath)
{
	if (InFolderPath.empty() || InFolderPath == "All" || InFolderPath == "Content")
	{
		return {};
	}

	return ResolveContentFilePath(Utf8GenericToFsPath(InFolderPath));
}

std::filesystem::path ResolveUAssetPathFromAssetPath(const std::string& InAssetPath)
{
	std::filesystem::path RelativePath = Utf8GenericToFsPath(InAssetPath);
	RelativePath.replace_extension(".uasset");
	return ResolveContentFilePath(RelativePath);
}

std::string FormatFilesystemErrorMessage(const std::error_code& InErrorCode)
{
	return "system error " + std::to_string(InErrorCode.value());
}

bool UpdateAssetAfterFolderDiskRename(
	const FAssetRegistryEntry& InEntry,
	const std::string& InNewAssetPath,
	std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}

	const std::filesystem::path NewUAssetPath = ResolveUAssetPathFromAssetPath(InNewAssetPath);
	if (!std::filesystem::exists(NewUAssetPath))
	{
		return RelocateAssetPath(InEntry, InNewAssetPath, OutErrorMessage);
	}

	FAssetPackageHeader Header;
	if (!UAssetSerializer::LoadHeader(NewUAssetPath, &Header, OutErrorMessage))
	{
		return false;
	}

	const std::string OldSoftPath = FSoftObjectPath::Build(InEntry.AssetPath, InEntry.ObjectName);
	ResourceRegistry::Get().UnregisterAssetByPath(std::filesystem::path(OldSoftPath));

	Header.AssetPath = InNewAssetPath;
	UStreamableRenderAsset* LoadedAsset = UAssetSerializer::LoadObject(NewUAssetPath, OutErrorMessage);
	if (LoadedAsset == nullptr)
	{
		return false;
	}

	LoadedAsset->SetAssetPath(InNewAssetPath);
	if (!UAssetSerializer::Save(Header, *LoadedAsset, NewUAssetPath, OutErrorMessage))
	{
		delete LoadedAsset;
		return false;
	}

	delete LoadedAsset;

	FAssetRegistry::Get().RemoveByAssetPath(InEntry.AssetPath);
	FAssetRegistry::Get().RegisterFromHeader(Header, NewUAssetPath);
	FAssetThumbnailService::Get().InvalidateEntry(InEntry);
	const std::string NewSoftPath = FSoftObjectPath::Build(Header.AssetPath, Header.ObjectName);
	FAssetRedirectStore::Get().RecordRedirect(OldSoftPath, NewSoftPath);
	return true;
}

bool RelocateFolderPath(
	const std::string& InFolderPath,
	const std::string& InNewFolderPath,
	std::string* OutErrorMessage)
{
	if (InFolderPath == InNewFolderPath)
	{
		return true;
	}

	std::vector<FAssetRegistryEntry> AffectedEntries;
	for (const FAssetRegistryEntry& Entry : FAssetRegistry::Get().ListAssets())
	{
		if (StartsWithPathPrefix(Entry.AssetPath, InFolderPath))
		{
			AffectedEntries.push_back(Entry);
		}
	}

	const std::filesystem::path OldDiskPath = ResolveFolderDiskPath(InFolderPath);
	const std::filesystem::path NewDiskPath = ResolveFolderDiskPath(InNewFolderPath);
	bool bRenamedFolderOnDisk = false;
	if (!OldDiskPath.empty() && OldDiskPath != NewDiskPath && std::filesystem::exists(OldDiskPath))
	{
		if (std::filesystem::exists(NewDiskPath))
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Target folder already exists: " + FsPathUtf8Generic(NewDiskPath);
			}
			return false;
		}

		std::error_code ErrorCode;
		std::filesystem::rename(OldDiskPath, NewDiskPath, ErrorCode);
		if (ErrorCode)
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Failed to rename folder on disk ("
					+ FsPathUtf8Generic(OldDiskPath) + " -> " + FsPathUtf8Generic(NewDiskPath)
					+ "): " + FormatFilesystemErrorMessage(ErrorCode);
			}
			return false;
		}

		bRenamedFolderOnDisk = true;
	}

	for (const FAssetRegistryEntry& Entry : AffectedEntries)
	{
		const std::string NewAssetPath = ReplacePathPrefix(Entry.AssetPath, InFolderPath, InNewFolderPath);
		if (bRenamedFolderOnDisk)
		{
			if (!UpdateAssetAfterFolderDiskRename(Entry, NewAssetPath, OutErrorMessage))
			{
				return false;
			}
		}
		else if (!RelocateAssetPath(Entry, NewAssetPath, OutErrorMessage))
		{
			return false;
		}
	}

	return true;
}
} // namespace

bool FAssetRenameService::IsValidRenameToken(const std::string& InName)
{
	if (InName.empty())
	{
		return false;
	}

	for (const char Character : InName)
	{
		if (Character == '/' || Character == '\\' || Character == ':' || Character == '*' || Character == '?'
			|| Character == '"' || Character == '<' || Character == '>' || Character == '|')
		{
			return false;
		}
	}

	return true;
}

bool FAssetRenameService::MoveAssetToFolder(
	const FAssetRegistryEntry& InEntry,
	const std::string& InTargetFolderPath,
	std::string* OutNewSoftObjectPath,
	std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}
	if (OutNewSoftObjectPath != nullptr)
	{
		OutNewSoftObjectPath->clear();
	}

	if (InTargetFolderPath.empty() || InTargetFolderPath == "All")
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Invalid target folder.";
		}
		return false;
	}

	const std::string NewObjectName = FAssetDuplicateService::FindUniqueObjectName(
		InTargetFolderPath,
		InEntry.ObjectName,
		InEntry.AssetPath);
	const std::string NewAssetPath = FAssetDuplicateService::BuildAssetPathInFolder(InTargetFolderPath, NewObjectName);
	if (NewAssetPath == InEntry.AssetPath)
	{
		if (OutNewSoftObjectPath != nullptr)
		{
			*OutNewSoftObjectPath = FSoftObjectPath::Build(InEntry.AssetPath, InEntry.ObjectName);
		}
		return true;
	}

	if (!IsValidRenameToken(NewObjectName))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Invalid asset name.";
		}
		return false;
	}

	FAssetPackageHeader Header;
	if (!UAssetSerializer::LoadHeader(InEntry.UAssetFilePath, &Header, OutErrorMessage))
	{
		return false;
	}

	std::filesystem::path NewRelativeUAssetPath(NewAssetPath);
	NewRelativeUAssetPath.replace_extension(".uasset");
	const std::filesystem::path NewUAssetPath = ResolveContentFilePath(NewRelativeUAssetPath);
	if (NewUAssetPath != InEntry.UAssetFilePath && std::filesystem::exists(NewUAssetPath))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Target asset already exists: " + NewUAssetPath.string();
		}
		return false;
	}

	UStreamableRenderAsset* LoadedAsset = UAssetSerializer::LoadObject(InEntry.UAssetFilePath, OutErrorMessage);
	if (LoadedAsset == nullptr)
	{
		return false;
	}

	const std::string OldSoftPath = FSoftObjectPath::Build(Header.AssetPath, Header.ObjectName);
	ResourceRegistry::Get().UnregisterAssetByPath(std::filesystem::path(OldSoftPath));

	Header.ObjectName = NewObjectName;
	Header.AssetPath = NewAssetPath;
	LoadedAsset->SetObjectName(NewObjectName);
	LoadedAsset->SetAssetPath(NewAssetPath);

	if (!UAssetSerializer::Save(Header, *LoadedAsset, NewUAssetPath, OutErrorMessage))
	{
		delete LoadedAsset;
		return false;
	}

	const std::filesystem::path OldMetaPath = UAssetSerializer::GetMetaPathForUAsset(InEntry.UAssetFilePath);
	const std::filesystem::path NewMetaPath = UAssetSerializer::GetMetaPathForUAsset(NewUAssetPath);
	std::error_code ErrorCode;
	if (std::filesystem::exists(OldMetaPath))
	{
		std::filesystem::create_directories(NewMetaPath.parent_path(), ErrorCode);
		std::filesystem::rename(OldMetaPath, NewMetaPath, ErrorCode);
	}

	if (NewUAssetPath != InEntry.UAssetFilePath)
	{
		std::filesystem::remove(InEntry.UAssetFilePath, ErrorCode);
	}

	delete LoadedAsset;

	FAssetRegistry::Get().RemoveByAssetPath(InEntry.AssetPath);
	FAssetRegistry::Get().RegisterFromHeader(Header, NewUAssetPath);
	FAssetThumbnailService::Get().InvalidateEntry(InEntry);
	const std::string NewSoftPath = FSoftObjectPath::Build(Header.AssetPath, Header.ObjectName);
	FAssetRedirectStore::Get().RecordRedirect(OldSoftPath, NewSoftPath);

	if (OutNewSoftObjectPath != nullptr)
	{
		*OutNewSoftObjectPath = NewSoftPath;
	}
	return true;
}

bool FAssetRenameService::RenameAssetObject(
	const FAssetRegistryEntry& InEntry,
	const std::string& InNewObjectName,
	std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}

	if (!IsValidRenameToken(InNewObjectName))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Invalid asset name.";
		}
		return false;
	}

	if (InNewObjectName == InEntry.ObjectName)
	{
		return true;
	}

	FAssetPackageHeader Header;
	if (!UAssetSerializer::LoadHeader(InEntry.UAssetFilePath, &Header, OutErrorMessage))
	{
		return false;
	}

	std::filesystem::path AssetPathPath(Header.AssetPath);
	const std::filesystem::path ParentPath = AssetPathPath.parent_path();
	const std::filesystem::path NewAssetPathPath = ParentPath / InNewObjectName;
	const std::string NewAssetPath = NewAssetPathPath.generic_string();

	std::filesystem::path NewRelativeUAssetPath = NewAssetPathPath;
	NewRelativeUAssetPath.replace_extension(".uasset");
	const std::filesystem::path NewUAssetPath = ResolveContentFilePath(NewRelativeUAssetPath);
	if (NewUAssetPath != InEntry.UAssetFilePath && std::filesystem::exists(NewUAssetPath))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Target asset already exists: " + NewUAssetPath.string();
		}
		return false;
	}

	UStreamableRenderAsset* LoadedAsset = UAssetSerializer::LoadObject(InEntry.UAssetFilePath, OutErrorMessage);
	if (LoadedAsset == nullptr)
	{
		return false;
	}

	const std::string OldSoftPath = FSoftObjectPath::Build(Header.AssetPath, Header.ObjectName);
	ResourceRegistry::Get().UnregisterAssetByPath(std::filesystem::path(OldSoftPath));

	Header.ObjectName = InNewObjectName;
	Header.AssetPath = NewAssetPath;
	LoadedAsset->SetObjectName(InNewObjectName);
	LoadedAsset->SetAssetPath(NewAssetPath);

	if (!UAssetSerializer::Save(Header, *LoadedAsset, NewUAssetPath, OutErrorMessage))
	{
		delete LoadedAsset;
		return false;
	}

	const std::filesystem::path OldMetaPath = UAssetSerializer::GetMetaPathForUAsset(InEntry.UAssetFilePath);
	const std::filesystem::path NewMetaPath = UAssetSerializer::GetMetaPathForUAsset(NewUAssetPath);
	std::error_code ErrorCode;
	if (std::filesystem::exists(OldMetaPath))
	{
		std::filesystem::create_directories(NewMetaPath.parent_path(), ErrorCode);
		std::filesystem::rename(OldMetaPath, NewMetaPath, ErrorCode);
	}

	if (NewUAssetPath != InEntry.UAssetFilePath)
	{
		std::filesystem::remove(InEntry.UAssetFilePath, ErrorCode);
	}

	delete LoadedAsset;

	FAssetRegistry::Get().RemoveByAssetPath(InEntry.AssetPath);
	FAssetRegistry::Get().RegisterFromHeader(Header, NewUAssetPath);
	FAssetThumbnailService::Get().InvalidateEntry(InEntry);
	const std::string NewSoftPath = FSoftObjectPath::Build(Header.AssetPath, Header.ObjectName);
	FAssetRedirectStore::Get().RecordRedirect(OldSoftPath, NewSoftPath);
	return true;
}

bool FAssetRenameService::RenameFolder(
	const std::string& InFolderPath,
	const std::string& InNewFolderName,
	std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}

	if (InFolderPath.empty() || InFolderPath == "All" || InFolderPath == "Content")
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Cannot rename root folder.";
		}
		return false;
	}

	if (!IsValidRenameToken(InNewFolderName))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Invalid folder name.";
		}
		return false;
	}

	const size_t LastSlash = InFolderPath.find_last_of('/');
	const std::string ParentPath = (LastSlash == std::string::npos) ? std::string() : InFolderPath.substr(0, LastSlash);
	const std::string OldFolderName =
		(LastSlash == std::string::npos) ? InFolderPath : InFolderPath.substr(LastSlash + 1);
	if (OldFolderName == InNewFolderName)
	{
		return true;
	}

	const std::string NewFolderPath =
		ParentPath.empty() ? InNewFolderName : ParentPath + "/" + InNewFolderName;

	return RelocateFolderPath(InFolderPath, NewFolderPath, OutErrorMessage);
}

bool FAssetRenameService::MoveFolder(
	const std::string& InSourceFolderPath,
	const std::string& InTargetFolderPath,
	std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}

	if (InSourceFolderPath.empty() || InSourceFolderPath == "All" || InSourceFolderPath == "Content")
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Cannot move protected folder.";
		}
		return false;
	}

	if (InTargetFolderPath.empty() || InTargetFolderPath == "All")
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Invalid target folder.";
		}
		return false;
	}

	if (InSourceFolderPath == InTargetFolderPath)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Cannot move folder into itself.";
		}
		return false;
	}

	if (StartsWithPathPrefix(InTargetFolderPath, InSourceFolderPath))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Cannot move folder into its subfolder.";
		}
		return false;
	}

	const size_t LastSlash = InSourceFolderPath.find_last_of('/');
	const std::string FolderName =
		(LastSlash == std::string::npos) ? InSourceFolderPath : InSourceFolderPath.substr(LastSlash + 1);
	const std::string NewFolderPath = (InTargetFolderPath == "Content")
		? FolderName
		: InTargetFolderPath + "/" + FolderName;

	return RelocateFolderPath(InSourceFolderPath, NewFolderPath, OutErrorMessage);
}

bool FAssetRenameService::CreateFolder(
	const std::string& InParentFolderPath,
	const std::string& InNewFolderName,
	std::string* OutNewFolderPath,
	std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}
	if (OutNewFolderPath != nullptr)
	{
		*OutNewFolderPath = "";
	}

	if (InParentFolderPath.empty() || InParentFolderPath == "All")
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Cannot create folder under root.";
		}
		return false;
	}

	if (!IsValidRenameToken(InNewFolderName))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Invalid folder name.";
		}
		return false;
	}

	const std::string NewFolderPath = (InParentFolderPath == "Content")
		? InNewFolderName
		: InParentFolderPath + "/" + InNewFolderName;

	const std::filesystem::path NewFolderNamePath = Utf8GenericToFsPath(InNewFolderName);
	std::filesystem::path DiskPath = (InParentFolderPath == "Content")
		? GProjectContentDirectory / NewFolderNamePath
		: ResolveContentFilePath(Utf8GenericToFsPath(InParentFolderPath)) / NewFolderNamePath;

	if (std::filesystem::exists(DiskPath))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Folder already exists: " + DiskPath.string();
		}
		return false;
	}

	std::error_code ErrorCode;
	std::filesystem::create_directories(DiskPath, ErrorCode);
	if (ErrorCode)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to create folder: " + ErrorCode.message();
		}
		return false;
	}

	if (OutNewFolderPath != nullptr)
	{
		*OutNewFolderPath = NewFolderPath;
	}
	return true;
}
