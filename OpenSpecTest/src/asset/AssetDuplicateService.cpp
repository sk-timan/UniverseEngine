#include "asset/AssetDuplicateService.h"

#include "asset/AssetPackageHeader.h"
#include "asset/AssetRegistry.h"
#include "asset/AssetSerializer.h"
#include "asset/ProjectPaths.h"
#include "asset/SoftObjectPath.h"
#include "render/asset/StreamableRenderAsset.h"

namespace
{
bool IsValidTargetFolder(const std::string& InFolderPath)
{
	return !InFolderPath.empty() && InFolderPath != "All";
}
} // namespace

std::string FAssetDuplicateService::BuildAssetPathInFolder(
	const std::string& InFolderPath,
	const std::string& InObjectName)
{
	if (InFolderPath == "Content")
	{
		return InObjectName;
	}

	return InFolderPath + "/" + InObjectName;
}

bool FAssetDuplicateService::AssetObjectNameExistsInFolder(
	const std::string& InFolderPath,
	const std::string& InObjectName,
	const std::string& InExcludeAssetPath)
{
	const std::string AssetPath = BuildAssetPathInFolder(InFolderPath, InObjectName);
	if (!InExcludeAssetPath.empty() && AssetPath == InExcludeAssetPath)
	{
		return false;
	}

	if (FAssetRegistry::Get().FindByAssetPath(AssetPath).has_value())
	{
		return true;
	}

	std::filesystem::path RelativeUAssetPath(AssetPath);
	RelativeUAssetPath.replace_extension(".uasset");
	const std::filesystem::path UAssetPath = ResolveContentFilePath(RelativeUAssetPath);
	return std::filesystem::exists(UAssetPath);
}

std::string FAssetDuplicateService::FindUniqueObjectName(
	const std::string& InFolderPath,
	const std::string& InBaseName,
	const std::string& InExcludeAssetPath)
{
	if (!AssetObjectNameExistsInFolder(InFolderPath, InBaseName, InExcludeAssetPath))
	{
		return InBaseName;
	}

	for (int SuffixIndex = 1; SuffixIndex < 10000; ++SuffixIndex)
	{
		const std::string CandidateName = InBaseName + "_" + std::to_string(SuffixIndex);
		if (!AssetObjectNameExistsInFolder(InFolderPath, CandidateName, InExcludeAssetPath))
		{
			return CandidateName;
		}
	}

	return InBaseName + "_1";
}

bool FAssetDuplicateService::DuplicateAsset(
	const FAssetRegistryEntry& InSourceEntry,
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

	if (!IsValidTargetFolder(InTargetFolderPath))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Invalid target folder.";
		}
		return false;
	}

	FAssetPackageHeader Header;
	if (!UAssetSerializer::LoadHeader(InSourceEntry.UAssetFilePath, &Header, OutErrorMessage))
	{
		return false;
	}

	const std::string NewObjectName = FindUniqueObjectName(InTargetFolderPath, InSourceEntry.ObjectName);

	const std::string NewAssetPath = BuildAssetPathInFolder(InTargetFolderPath, NewObjectName);
	std::filesystem::path NewRelativeUAssetPath(NewAssetPath);
	NewRelativeUAssetPath.replace_extension(".uasset");
	const std::filesystem::path NewUAssetPath = ResolveContentFilePath(NewRelativeUAssetPath);
	if (std::filesystem::exists(NewUAssetPath))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Target asset already exists: " + NewUAssetPath.string();
		}
		return false;
	}

	UStreamableRenderAsset* LoadedAsset = UAssetSerializer::LoadObject(InSourceEntry.UAssetFilePath, OutErrorMessage);
	if (LoadedAsset == nullptr)
	{
		return false;
	}

	Header.Guid = GenerateAssetGuid();
	Header.AssetPath = NewAssetPath;
	Header.ObjectName = NewObjectName;
	LoadedAsset->SetAssetPath(NewAssetPath);
	LoadedAsset->SetObjectName(NewObjectName);

	if (!UAssetSerializer::Save(Header, *LoadedAsset, NewUAssetPath, OutErrorMessage))
	{
		delete LoadedAsset;
		return false;
	}

	const std::filesystem::path SourceMetaPath = UAssetSerializer::GetMetaPathForUAsset(InSourceEntry.UAssetFilePath);
	const std::filesystem::path NewMetaPath = UAssetSerializer::GetMetaPathForUAsset(NewUAssetPath);
	if (std::filesystem::exists(SourceMetaPath))
	{
		FAssetMeta Meta;
		if (UAssetSerializer::LoadMeta(SourceMetaPath, &Meta, OutErrorMessage))
		{
			(void)UAssetSerializer::SaveMeta(Meta, NewMetaPath, OutErrorMessage);
		}
	}

	delete LoadedAsset;

	FAssetRegistry::Get().RegisterFromHeader(Header, NewUAssetPath);

	const std::string NewSoftPath = FSoftObjectPath::Build(Header.AssetPath, Header.ObjectName);
	if (OutNewSoftObjectPath != nullptr)
	{
		*OutNewSoftObjectPath = NewSoftPath;
	}
	return true;
}
