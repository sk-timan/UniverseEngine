#include "asset/MeshImportFactory.h"

#include <nlohmann/json.hpp>

#include "asset/AssetManager.h"
#include "asset/AssetRegistry.h"
#include "asset/AssetSerializer.h"
#include "asset/ProjectPaths.h"
#include "asset/SoftObjectPath.h"
#include "data/MeshImporter.h"
#include "render/ResourceRegistry.h"
#include "render/asset/SkeletalMesh.h"
#include "render/asset/StaticMesh.h"

namespace
{
FAssetMeta BuildMeta(const std::filesystem::path& InSourceFile)
{
	FAssetMeta Meta;
	Meta.SourceFile = std::filesystem::weakly_canonical(InSourceFile).string();
	Meta.SourceTimestamp = GetFileTimestampIso(InSourceFile);
	Meta.ImportSettings = nlohmann::json::object();
	Meta.ImportHash = ComputeImportHash(Meta.SourceFile, Meta.SourceTimestamp);
	return Meta;
}
} // namespace

void UMeshImportFactory::RemovePartialUAssetFiles(const std::filesystem::path& InUAssetPath)
{
	std::error_code ErrorCode;
	std::filesystem::remove(InUAssetPath, ErrorCode);
	std::filesystem::remove(UAssetSerializer::GetMetaPathForUAsset(InUAssetPath), ErrorCode);
}

bool UMeshImportFactory::SaveMeshAsset(
	const FAssetPackageHeader& InHeader,
	UStreamableRenderAsset* InAsset,
	const FAssetMeta& InMeta,
	std::string* OutErrorMessage)
{
	if (InAsset == nullptr)
	{
		return false;
	}

	const std::filesystem::path UAssetPath = ResolveContentFilePath(FSoftObjectPath::Parse(
		FSoftObjectPath::Build(InHeader.AssetPath, InHeader.ObjectName)).ToUAssetRelativePath());

	RemovePartialUAssetFiles(UAssetPath);

	if (!UAssetSerializer::Save(InHeader, *InAsset, UAssetPath, OutErrorMessage))
	{
		RemovePartialUAssetFiles(UAssetPath);
		return false;
	}

	if (!UAssetSerializer::SaveMeta(InMeta, UAssetSerializer::GetMetaPathForUAsset(UAssetPath), OutErrorMessage))
	{
		RemovePartialUAssetFiles(UAssetPath);
		return false;
	}

	FAssetRegistry::Get().RegisterFromHeader(InHeader, UAssetPath);
	return true;
}

UStaticMesh* UMeshImportFactory::ImportStaticMeshAndSave(
	const FMeshImportRequest& InRequest,
	std::string* OutSoftObjectPath,
	std::string* OutErrorMessage)
{
	if (OutSoftObjectPath != nullptr)
	{
		*OutSoftObjectPath = "";
	}

	const std::string SoftPath = FSoftObjectPath::Build(InRequest.AssetPath, InRequest.ObjectName);
	MeshImporter Importer;
	UStaticMesh* StaticMesh = Importer.ImportStaticMesh(InRequest.SourceFile, InRequest.ObjectName);
	if (StaticMesh == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Assimp failed to import static mesh: " + MeshImporter::GetLastAssimpError();
		}
		return nullptr;
	}

	StaticMesh->SetAssetPath(InRequest.AssetPath);
	StaticMesh->SetObjectName(InRequest.ObjectName);
	ResourceRegistry::Get().RegisterAsset(StaticMesh);

	FAssetPackageHeader Header;
	Header.Guid = GenerateAssetGuid();
	Header.Type = "StaticMesh";
	Header.AssetPath = InRequest.AssetPath;
	Header.ObjectName = InRequest.ObjectName;

	const FAssetMeta Meta = BuildMeta(InRequest.SourceFile);
	if (!SaveMeshAsset(Header, StaticMesh, Meta, OutErrorMessage))
	{
		delete StaticMesh;
		ResourceRegistry::Get().UnregisterAsset(InRequest.AssetPath);
		return nullptr;
	}

	if (OutSoftObjectPath != nullptr)
	{
		*OutSoftObjectPath = SoftPath;
	}
	return StaticMesh;
}

USkeletalMesh* UMeshImportFactory::ImportSkeletalMeshAndSave(
	const FMeshImportRequest& InRequest,
	std::string* OutSoftObjectPath,
	std::string* OutErrorMessage)
{
	if (OutSoftObjectPath != nullptr)
	{
		*OutSoftObjectPath = "";
	}

	const std::string SoftPath = FSoftObjectPath::Build(InRequest.AssetPath, InRequest.ObjectName);
	MeshImporter Importer;
	USkeletalMesh* SkeletalMesh = Importer.ImportSkeletalMesh(InRequest.SourceFile, InRequest.ObjectName);
	if (SkeletalMesh == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Assimp failed to import skeletal mesh: " + MeshImporter::GetLastAssimpError();
		}
		return nullptr;
	}

	SkeletalMesh->SetAssetPath(InRequest.AssetPath);
	SkeletalMesh->SetObjectName(InRequest.ObjectName);
	ResourceRegistry::Get().RegisterAsset(SkeletalMesh);

	FAssetPackageHeader Header;
	Header.Guid = GenerateAssetGuid();
	Header.Type = "SkeletalMesh";
	Header.AssetPath = InRequest.AssetPath;
	Header.ObjectName = InRequest.ObjectName;

	const FAssetMeta Meta = BuildMeta(InRequest.SourceFile);
	if (!SaveMeshAsset(Header, SkeletalMesh, Meta, OutErrorMessage))
	{
		delete SkeletalMesh;
		ResourceRegistry::Get().UnregisterAsset(InRequest.AssetPath);
		return nullptr;
	}

	if (OutSoftObjectPath != nullptr)
	{
		*OutSoftObjectPath = SoftPath;
	}
	return SkeletalMesh;
}

bool UMeshImportFactory::Reimport(
	const std::string& InSoftObjectPath,
	const std::filesystem::path& InSourceFile,
	std::string* OutErrorMessage)
{
	const auto RegistryEntry = FAssetRegistry::Get().FindBySoftPath(InSoftObjectPath);
	if (!RegistryEntry.has_value())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Asset not found in registry: " + InSoftObjectPath;
		}
		return false;
	}

	FAssetPackageHeader Header;
	if (!UAssetSerializer::LoadHeader(RegistryEntry->UAssetFilePath, &Header, OutErrorMessage))
	{
		return false;
	}

	UAssetManager::Get().Unload(InSoftObjectPath);

	MeshImporter Importer;
	UStreamableRenderAsset* ImportedAsset = nullptr;
	if (Header.Type == "SkeletalMesh")
	{
		ImportedAsset = Importer.ImportSkeletalMesh(InSourceFile, Header.ObjectName);
	}
	else
	{
		ImportedAsset = Importer.ImportStaticMesh(InSourceFile, Header.ObjectName);
	}

	if (ImportedAsset == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Assimp failed to reimport: " + MeshImporter::GetLastAssimpError();
		}
		return false;
	}

	ImportedAsset->SetAssetPath(Header.AssetPath);
	ImportedAsset->SetObjectName(Header.ObjectName);
	ResourceRegistry::Get().RegisterAsset(ImportedAsset);

	const FAssetMeta Meta = BuildMeta(InSourceFile);
	if (!SaveMeshAsset(Header, ImportedAsset, Meta, OutErrorMessage))
	{
		return false;
	}

	return true;
}
