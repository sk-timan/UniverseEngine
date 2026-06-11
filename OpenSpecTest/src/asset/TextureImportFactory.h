#pragma once

#include <filesystem>
#include <string>

#include "asset/AssetPackageHeader.h"
#include "render/asset/TextureData.h"

class UTexture2D;

struct FTextureImportRequest
{
	std::filesystem::path SourceFile;
	std::string AssetPath;
	std::string ObjectName;
	FTextureImportSettings ImportSettings;
};

class UTextureImportFactory
{
public:
	static UTexture2D* ImportTexture2DAndSave(
		const FTextureImportRequest& InRequest,
		std::string* OutSoftObjectPath,
		std::string* OutErrorMessage);

	static bool Reimport(
		const std::string& InSoftObjectPath,
		const std::filesystem::path& InSourceFile,
		std::string* OutErrorMessage);

private:
	static bool CookTextureFromSource(
		const std::filesystem::path& InSourceFile,
		const FTextureImportSettings& InSettings,
		UTexture2D* OutTexture,
		std::string* OutErrorMessage);

	static bool SaveTextureAsset(
		const FAssetPackageHeader& InHeader,
		UTexture2D* InAsset,
		const FAssetMeta& InMeta,
		std::string* OutErrorMessage);

	static void RemovePartialUAssetFiles(const std::filesystem::path& InUAssetPath);
};
