#include "asset/TextureImportFactory.h"

#include <nlohmann/json.hpp>
#include <utility>

#include "asset/AssetManager.h"
#include "asset/AssetRegistry.h"
#include "asset/AssetSerializer.h"
#include "asset/ProjectPaths.h"
#include "asset/SoftObjectPath.h"
#include "data/ImageDecoder.h"
#include "data/TextureCookUtils.h"
#include "render/ResourceRegistry.h"
#include "render/asset/Texture2D.h"

namespace
{
FAssetMeta BuildMeta(const std::filesystem::path& InSourceFile, const FTextureImportSettings& InSettings)
{
	FAssetMeta Meta;
	Meta.SourceFile = std::filesystem::weakly_canonical(InSourceFile).string();
	Meta.SourceTimestamp = GetFileTimestampIso(InSourceFile);
	Meta.ImportSettings = nlohmann::json{
		{"sRGB", InSettings.bSRGB},
		{"flip_y", InSettings.bFlipY},
		{"max_size", InSettings.MaxSize},
	};
	Meta.ImportHash = ComputeImportHash(Meta.SourceFile, Meta.SourceTimestamp);
	return Meta;
}
} // namespace

void UTextureImportFactory::RemovePartialUAssetFiles(const std::filesystem::path& InUAssetPath)
{
	std::error_code ErrorCode;
	std::filesystem::remove(InUAssetPath, ErrorCode);
	std::filesystem::remove(UAssetSerializer::GetMetaPathForUAsset(InUAssetPath), ErrorCode);
}

bool UTextureImportFactory::CookTextureFromSource(
	const std::filesystem::path& InSourceFile,
	const FTextureImportSettings& InSettings,
	UTexture2D* OutTexture,
	std::string* OutErrorMessage)
{
	if (OutTexture == nullptr)
	{
		return false;
	}

	FDecodedImage DecodedImage;
	if (!FImageDecoder::DecodeFromFile(InSourceFile, InSettings.bFlipY, &DecodedImage, OutErrorMessage))
	{
		return false;
	}

	const FTextureSource Source = FImageDecoder::ToTextureSource(DecodedImage);
	FTexturePlatformData PlatformData;
	if (!FTextureCookUtils::BuildPlatformData(
			Source,
			InSettings.MaxSize,
			&PlatformData,
			OutErrorMessage))
	{
		return false;
	}

	OutTexture->SetSRGB(InSettings.bSRGB);
	OutTexture->SetSource(Source);
	OutTexture->SetPlatformData(std::move(PlatformData));
	return true;
}

bool UTextureImportFactory::SaveTextureAsset(
	const FAssetPackageHeader& InHeader,
	UTexture2D* InAsset,
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

UTexture2D* UTextureImportFactory::ImportTexture2DAndSave(
	const FTextureImportRequest& InRequest,
	std::string* OutSoftObjectPath,
	std::string* OutErrorMessage)
{
	if (OutSoftObjectPath != nullptr)
	{
		*OutSoftObjectPath = "";
	}

	const std::string SoftPath = FSoftObjectPath::Build(InRequest.AssetPath, InRequest.ObjectName);
	auto Texture = std::make_unique<UTexture2D>(0, InRequest.ObjectName, nullptr);
	if (!CookTextureFromSource(InRequest.SourceFile, InRequest.ImportSettings, Texture.get(), OutErrorMessage))
	{
		return nullptr;
	}

	Texture->SetAssetPath(InRequest.AssetPath);
	Texture->SetObjectName(InRequest.ObjectName);
	ResourceRegistry::Get().RegisterAsset(Texture.get());

	FAssetPackageHeader Header;
	Header.Guid = GenerateAssetGuid();
	Header.Type = "Texture2D";
	Header.AssetPath = InRequest.AssetPath;
	Header.ObjectName = InRequest.ObjectName;

	const FAssetMeta Meta = BuildMeta(InRequest.SourceFile, InRequest.ImportSettings);
	if (!SaveTextureAsset(Header, Texture.get(), Meta, OutErrorMessage))
	{
		ResourceRegistry::Get().UnregisterAsset(InRequest.AssetPath);
		return nullptr;
	}

	if (OutSoftObjectPath != nullptr)
	{
		*OutSoftObjectPath = SoftPath;
	}

	return Texture.release();
}

bool UTextureImportFactory::Reimport(
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

	if (Header.Type != "Texture2D")
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Asset is not Texture2D: " + Header.Type;
		}
		return false;
	}

	UAssetManager::Get().Unload(InSoftObjectPath);

	FTextureImportSettings ImportSettings;
	FAssetMeta ExistingMeta;
	if (UAssetSerializer::LoadMeta(
			UAssetSerializer::GetMetaPathForUAsset(RegistryEntry->UAssetFilePath),
			&ExistingMeta,
			nullptr))
	{
		if (ExistingMeta.ImportSettings.contains("sRGB"))
		{
			ImportSettings.bSRGB = ExistingMeta.ImportSettings.at("sRGB").get<bool>();
		}
		if (ExistingMeta.ImportSettings.contains("flip_y"))
		{
			ImportSettings.bFlipY = ExistingMeta.ImportSettings.at("flip_y").get<bool>();
		}
		if (ExistingMeta.ImportSettings.contains("max_size"))
		{
			ImportSettings.MaxSize = ExistingMeta.ImportSettings.at("max_size").get<int32_t>();
		}
	}

	auto Texture = std::make_unique<UTexture2D>(0, Header.ObjectName, nullptr);
	if (!CookTextureFromSource(InSourceFile, ImportSettings, Texture.get(), OutErrorMessage))
	{
		return false;
	}

	Texture->SetAssetPath(Header.AssetPath);
	Texture->SetObjectName(Header.ObjectName);
	ResourceRegistry::Get().RegisterAsset(Texture.get());

	const FAssetMeta Meta = BuildMeta(InSourceFile, ImportSettings);
	if (!SaveTextureAsset(Header, Texture.get(), Meta, OutErrorMessage))
	{
		return false;
	}

	return true;
}
