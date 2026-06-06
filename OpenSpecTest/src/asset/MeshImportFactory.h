#pragma once

#include <filesystem>
#include <string>

#include "asset/AssetPackageHeader.h"

class UStaticMesh;
class USkeletalMesh;
class UStreamableRenderAsset;

struct FMeshImportRequest
{
	std::filesystem::path SourceFile;
	std::string AssetPath;
	std::string ObjectName;
};

class UMeshImportFactory
{
public:
	static UStaticMesh* ImportStaticMeshAndSave(
		const FMeshImportRequest& InRequest,
		std::string* OutSoftObjectPath,
		std::string* OutErrorMessage);

	static USkeletalMesh* ImportSkeletalMeshAndSave(
		const FMeshImportRequest& InRequest,
		std::string* OutSoftObjectPath,
		std::string* OutErrorMessage);

	static bool Reimport(
		const std::string& InSoftObjectPath,
		const std::filesystem::path& InSourceFile,
		std::string* OutErrorMessage);

private:
	static bool SaveMeshAsset(
		const FAssetPackageHeader& InHeader,
		UStreamableRenderAsset* InAsset,
		const FAssetMeta& InMeta,
		std::string* OutErrorMessage);

	static void RemovePartialUAssetFiles(const std::filesystem::path& InUAssetPath);
};
