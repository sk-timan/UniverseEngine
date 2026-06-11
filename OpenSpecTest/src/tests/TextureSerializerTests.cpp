#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

#include "asset/AssetPackageHeader.h"
#include "asset/AssetManager.h"
#include "asset/AssetRegistry.h"
#include "asset/AssetSerializer.h"
#include "asset/ProjectPaths.h"
#include "asset/SoftObjectPath.h"
#include "render/Dx12Renderer.h"
#include "render/ResourceRegistry.h"
#include "render/asset/Texture2D.h"
#include "render/asset/StreamableRenderAsset.h"

namespace
{
bool AssertTrue(bool bCondition, const std::string& InMessage)
{
	if (!bCondition)
	{
		std::cerr << "ASSERT FAILED: " << InMessage << '\n';
		return false;
	}
	return true;
}

uint64_t HashPixelData(const std::vector<uint8_t>& InData)
{
	uint64_t Hash = 1469598103934665603ULL;
	for (const uint8_t Byte : InData)
	{
		Hash ^= Byte;
		Hash *= 1099511628211ULL;
	}
	return Hash;
}

UTexture2D* CreateTestTexture2D()
{
	auto* Texture = new UTexture2D(0, "TestTexture", nullptr);
	Texture->SetAssetPath("Textures/Test/TestTexture");
	Texture->SetSRGB(true);

	FTexturePlatformData PlatformData;
	PlatformData.Width = 2;
	PlatformData.Height = 2;
	PlatformData.Format = ETexturePixelFormat::RGBA8;
	PlatformData.MipCount = 1;

	FTextureMipLevel Mip0;
	Mip0.Width = 2;
	Mip0.Height = 2;
	Mip0.Data = {
		255, 0, 0, 255,
		0, 255, 0, 255,
		0, 0, 255, 255,
		255, 255, 0, 255,
	};
	PlatformData.Mips.push_back(std::move(Mip0));
	Texture->SetPlatformData(std::move(PlatformData));
	return Texture;
}

void ResetLoadedAssets()
{
	UAssetManager::Get().ClearLoadedAssets();
}

bool SaveTestTextureUAsset(
	const std::filesystem::path& InUAssetPath,
	const std::string& InAssetPath,
	const std::string& InObjectName,
	std::string* OutSoftObjectPath,
	uint64_t* OutSourcePixelHash,
	std::string* OutErrorMessage)
{
	if (OutSoftObjectPath != nullptr)
	{
		OutSoftObjectPath->clear();
	}
	if (OutSourcePixelHash != nullptr)
	{
		*OutSourcePixelHash = 0;
	}

	std::error_code ErrorCode;
	const std::filesystem::path Parent = InUAssetPath.parent_path();
	if (!Parent.empty())
	{
		std::filesystem::create_directories(Parent, ErrorCode);
	}

	std::unique_ptr<UTexture2D> SourceTexture(CreateTestTexture2D());
	SourceTexture->SetAssetPath(InAssetPath);
	SourceTexture->SetObjectName(InObjectName);

	if (OutSourcePixelHash != nullptr)
	{
		const FTextureMipLevel* Mip0 = SourceTexture->GetPlatformData().GetMip(0);
		if (Mip0 != nullptr)
		{
			*OutSourcePixelHash = HashPixelData(Mip0->Data);
		}
	}

	FAssetPackageHeader Header;
	Header.Guid = GenerateAssetGuid();
	Header.Type = "Texture2D";
	Header.AssetPath = InAssetPath;
	Header.ObjectName = InObjectName;

	if (!UAssetSerializer::Save(Header, *SourceTexture, InUAssetPath, OutErrorMessage))
	{
		return false;
	}

	if (OutSoftObjectPath != nullptr)
	{
		*OutSoftObjectPath = FSoftObjectPath::Build(InAssetPath, InObjectName);
	}
	return true;
}

bool AssertLoadedTextureMatches(
	UTexture2D* InTexture,
	uint64_t InExpectedPixelHash,
	const std::string& InContext)
{
	if (!AssertTrue(InTexture != nullptr, InContext + ": asset is null"))
	{
		return false;
	}
	if (!AssertTrue(InTexture->HasResidentPlatformData(), InContext + ": missing platform data"))
	{
		return false;
	}
	if (!AssertTrue(InTexture->GetSizeX() == 2, InContext + ": width mismatch"))
	{
		return false;
	}
	if (!AssertTrue(InTexture->GetSizeY() == 2, InContext + ": height mismatch"))
	{
		return false;
	}
	if (!AssertTrue(InTexture->GetMipCount() == 1, InContext + ": mip count mismatch"))
	{
		return false;
	}

	const FTextureMipLevel* Mip0 = InTexture->GetPlatformData().GetMip(0);
	if (!AssertTrue(Mip0 != nullptr, InContext + ": missing mip0"))
	{
		return false;
	}
	if (!AssertTrue(HashPixelData(Mip0->Data) == InExpectedPixelHash, InContext + ": pixel hash mismatch"))
	{
		return false;
	}
	return true;
}

bool TestGetOrLoadTextureViaRegistry()
{
	ResetLoadedAssets();

	const std::filesystem::path TempDir =
		std::filesystem::temp_directory_path() / "openspec_texture_getorload_registry";
	std::error_code ErrorCode;
	std::filesystem::remove_all(TempDir, ErrorCode);

	const std::string AssetPath = "Textures/Test/GetOrLoadRegistry";
	const std::string ObjectName = "GetOrLoadRegistry";
	const std::filesystem::path UAssetPath = TempDir / "GetOrLoadRegistry.uasset";

	std::string SoftPath;
	uint64_t SourcePixelHash = 0;
	std::string Error;
	if (!SaveTestTextureUAsset(UAssetPath, AssetPath, ObjectName, &SoftPath, &SourcePixelHash, &Error))
	{
		return AssertTrue(false, "SaveTestTextureUAsset failed: " + Error);
	}

	FAssetPackageHeader Header;
	if (!UAssetSerializer::LoadHeader(UAssetPath, &Header, &Error))
	{
		return AssertTrue(false, "LoadHeader failed: " + Error);
	}
	FAssetRegistry::Get().RegisterFromHeader(Header, UAssetPath);

	UStreamableRenderAsset* LoadedAsset = UAssetManager::Get().GetOrLoad(SoftPath, &Error);
	auto* LoadedTexture = dynamic_cast<UTexture2D*>(LoadedAsset);
	if (!AssertLoadedTextureMatches(LoadedTexture, SourcePixelHash, "GetOrLoad via registry"))
	{
		return false;
	}

	std::filesystem::remove_all(TempDir, ErrorCode);
	FAssetRegistry::Get().RemoveByAssetPath(AssetPath);
	ResetLoadedAssets();
	return true;
}

bool TestGetOrLoadTextureCacheHit()
{
	ResetLoadedAssets();

	const std::filesystem::path TempDir =
		std::filesystem::temp_directory_path() / "openspec_texture_getorload_cache";
	std::error_code ErrorCode;
	std::filesystem::remove_all(TempDir, ErrorCode);

	const std::string AssetPath = "Textures/Test/GetOrLoadCache";
	const std::string ObjectName = "GetOrLoadCache";
	const std::filesystem::path UAssetPath = TempDir / "GetOrLoadCache.uasset";

	std::string SoftPath;
	uint64_t SourcePixelHash = 0;
	std::string Error;
	if (!SaveTestTextureUAsset(UAssetPath, AssetPath, ObjectName, &SoftPath, &SourcePixelHash, &Error))
	{
		return AssertTrue(false, "SaveTestTextureUAsset failed: " + Error);
	}

	FAssetPackageHeader Header;
	if (!UAssetSerializer::LoadHeader(UAssetPath, &Header, &Error))
	{
		return AssertTrue(false, "LoadHeader failed: " + Error);
	}
	FAssetRegistry::Get().RegisterFromHeader(Header, UAssetPath);

	UStreamableRenderAsset* FirstLoad = UAssetManager::Get().GetOrLoad(SoftPath, &Error);
	if (!AssertTrue(FirstLoad != nullptr, "First GetOrLoad failed: " + Error))
	{
		return false;
	}

	UStreamableRenderAsset* SecondLoad = UAssetManager::Get().GetOrLoad(SoftPath, &Error);
	if (!AssertTrue(SecondLoad != nullptr, "Second GetOrLoad failed: " + Error))
	{
		return false;
	}
	if (!AssertTrue(FirstLoad == SecondLoad, "GetOrLoad cache hit should return the same instance"))
	{
		return false;
	}

	auto* CachedTexture = dynamic_cast<UTexture2D*>(SecondLoad);
	if (!AssertTrue(
			CachedTexture != nullptr && CachedTexture->HasResidentPlatformData(),
			"Cached texture should retain resident platform data"))
	{
		return false;
	}

	std::filesystem::remove_all(TempDir, ErrorCode);
	FAssetRegistry::Get().RemoveByAssetPath(AssetPath);
	ResetLoadedAssets();
	return true;
}

bool TestGetOrLoadTextureWithoutSourceOrMeta()
{
	ResetLoadedAssets();

	const std::filesystem::path TempDir =
		std::filesystem::temp_directory_path() / "openspec_texture_getorload_no_source";
	std::error_code ErrorCode;
	std::filesystem::remove_all(TempDir, ErrorCode);

	const std::string AssetPath = "Textures/Test/GetOrLoadNoSource";
	const std::string ObjectName = "GetOrLoadNoSource";
	const std::filesystem::path UAssetPath = TempDir / "GetOrLoadNoSource.uasset";
	const std::filesystem::path MetaPath = UAssetSerializer::GetMetaPathForUAsset(UAssetPath);
	const std::filesystem::path FakeSourcePath = TempDir / "missing_source.png";

	std::string SoftPath;
	uint64_t SourcePixelHash = 0;
	std::string Error;
	if (!SaveTestTextureUAsset(UAssetPath, AssetPath, ObjectName, &SoftPath, &SourcePixelHash, &Error))
	{
		return AssertTrue(false, "SaveTestTextureUAsset failed: " + Error);
	}

	std::filesystem::remove(MetaPath, ErrorCode);
	if (!AssertTrue(!std::filesystem::exists(MetaPath), "Meta file should not exist for this test"))
	{
		return false;
	}
	if (!AssertTrue(!std::filesystem::exists(FakeSourcePath), "Source png must not exist for this test"))
	{
		return false;
	}

	FAssetPackageHeader Header;
	if (!UAssetSerializer::LoadHeader(UAssetPath, &Header, &Error))
	{
		return AssertTrue(false, "LoadHeader failed: " + Error);
	}

	FAssetRegistry::Get().RegisterFromHeader(Header, UAssetPath);

	UStreamableRenderAsset* LoadedAsset = UAssetManager::Get().GetOrLoad(SoftPath, &Error);
	auto* LoadedTexture = dynamic_cast<UTexture2D*>(LoadedAsset);
	if (!AssertLoadedTextureMatches(
			LoadedTexture,
			SourcePixelHash,
			"GetOrLoad without source/meta"))
	{
		return false;
	}

	std::filesystem::remove_all(TempDir, ErrorCode);
	FAssetRegistry::Get().RemoveByAssetPath(AssetPath);
	ResetLoadedAssets();
	return true;
}

bool TestGetOrLoadTextureViaContentPath()
{
	ResetLoadedAssets();

	const std::string AssetPath = "Textures/Test/GetOrLoadContent";
	const std::string ObjectName = "GetOrLoadContent";
	const std::filesystem::path UAssetPath =
		ResolveContentFilePath(FSoftObjectPath::Parse(FSoftObjectPath::Build(AssetPath, ObjectName)).ToUAssetRelativePath());

	std::error_code ErrorCode;
	if (UAssetPath.has_parent_path())
	{
		std::filesystem::create_directories(UAssetPath.parent_path(), ErrorCode);
	}

	std::string SoftPath;
	uint64_t SourcePixelHash = 0;
	std::string Error;
	if (!SaveTestTextureUAsset(UAssetPath, AssetPath, ObjectName, &SoftPath, &SourcePixelHash, &Error))
	{
		return AssertTrue(false, "SaveTestTextureUAsset failed: " + Error);
	}

	FAssetRegistry::Get().RemoveByAssetPath(AssetPath);

	UStreamableRenderAsset* LoadedAsset = UAssetManager::Get().GetOrLoad(SoftPath, &Error);
	auto* LoadedTexture = dynamic_cast<UTexture2D*>(LoadedAsset);
	if (!AssertLoadedTextureMatches(LoadedTexture, SourcePixelHash, "GetOrLoad via content path"))
	{
		std::filesystem::remove(UAssetPath, ErrorCode);
		std::filesystem::remove(UAssetSerializer::GetMetaPathForUAsset(UAssetPath), ErrorCode);
		ResetLoadedAssets();
		return false;
	}

	std::filesystem::remove(UAssetPath, ErrorCode);
	std::filesystem::remove(UAssetSerializer::GetMetaPathForUAsset(UAssetPath), ErrorCode);
	ResetLoadedAssets();
	return true;
}

bool TestGetOrLoadTextureFailureCases()
{
	ResetLoadedAssets();

	std::string Error;
	if (!AssertTrue(UAssetManager::Get().GetOrLoad("", &Error) == nullptr, "Empty SoftObjectPath should fail"))
	{
		return false;
	}
	if (!AssertTrue(!Error.empty(), "Empty SoftObjectPath should set error message"))
	{
		return false;
	}

	const std::string MissingSoftPath = "Textures/Test/DoesNotExist.MissingTexture";
	UStreamableRenderAsset* MissingAsset = UAssetManager::Get().GetOrLoad(MissingSoftPath, &Error);
	if (!AssertTrue(MissingAsset == nullptr, "Missing uasset should return nullptr"))
	{
		return false;
	}
	if (!AssertTrue(!Error.empty(), "Missing uasset should set error message"))
	{
		return false;
	}

	ResetLoadedAssets();
	return true;
}

bool TestTexture2DRoundTrip()
{
	const std::filesystem::path TempDir =
		std::filesystem::temp_directory_path() / "openspec_texture_serializer_test";
	std::error_code ErrorCode;
	std::filesystem::remove_all(TempDir, ErrorCode);
	std::filesystem::create_directories(TempDir, ErrorCode);

	const std::filesystem::path UAssetPath = TempDir / "TestTexture.uasset";

	std::unique_ptr<UTexture2D> SourceTexture(CreateTestTexture2D());
	const uint64_t SourcePixelHash = HashPixelData(SourceTexture->GetPlatformData().GetMip(0)->Data);

	FAssetPackageHeader Header;
	Header.Guid = GenerateAssetGuid();
	Header.Type = "Texture2D";
	Header.AssetPath = "Textures/Test/TestTexture";
	Header.ObjectName = "TestTexture";

	std::string Error;
	if (!AssertTrue(
			UAssetSerializer::Save(Header, *SourceTexture, UAssetPath, &Error),
			"Save uasset failed: " + Error))
	{
		return false;
	}

	std::unique_ptr<UStreamableRenderAsset> LoadedAsset(UAssetSerializer::LoadObject(UAssetPath, &Error));
	auto* LoadedTexture = dynamic_cast<UTexture2D*>(LoadedAsset.get());
	if (!AssertTrue(LoadedTexture != nullptr, "LoadObject did not return Texture2D: " + Error))
	{
		return false;
	}

	if (!AssertTrue(LoadedTexture->GetSizeX() == 2, "Width mismatch"))
	{
		return false;
	}
	if (!AssertTrue(LoadedTexture->GetSizeY() == 2, "Height mismatch"))
	{
		return false;
	}
	if (!AssertTrue(LoadedTexture->GetMipCount() == 1, "Mip count mismatch"))
	{
		return false;
	}
	if (!AssertTrue(LoadedTexture->HasResidentPlatformData(), "Missing platform data"))
	{
		return false;
	}

	const FTextureMipLevel* LoadedMip0 = LoadedTexture->GetPlatformData().GetMip(0);
	if (!AssertTrue(LoadedMip0 != nullptr, "Missing mip0"))
	{
		return false;
	}

	if (!AssertTrue(
			HashPixelData(LoadedMip0->Data) == SourcePixelHash,
			"Pixel hash mismatch after round trip"))
	{
		return false;
	}

	std::filesystem::remove_all(TempDir, ErrorCode);
	return true;
}

bool TestInitResourceCreatesGpuResource()
{
	std::unique_ptr<UTexture2D> Texture(CreateTestTexture2D());
	if (!AssertTrue(Texture->HasResidentPlatformData(), "Test texture missing platform data"))
	{
		return false;
	}
	if (!AssertTrue(!Texture->HasResidentTextureResource(), "GPU resource should not exist before InitResource"))
	{
		return false;
	}

	Dx12Renderer Renderer;
	Texture->InitResource(&Renderer);
	if (!AssertTrue(Texture->HasResidentTextureResource(), "HasResidentTextureResource should be true after InitResource"))
	{
		return false;
	}

	Texture->ReleaseResource();
	if (!AssertTrue(!Texture->HasResidentTextureResource(), "GPU resource should be released after ReleaseResource"))
	{
		return false;
	}

	return true;
}

bool TestLoadedTextureInitResourceCreatesGpuResource()
{
	const std::filesystem::path TempDir =
		std::filesystem::temp_directory_path() / "openspec_texture_init_resource_test";
	std::error_code ErrorCode;
	std::filesystem::remove_all(TempDir, ErrorCode);
	std::filesystem::create_directories(TempDir, ErrorCode);

	const std::filesystem::path UAssetPath = TempDir / "TestTexture.uasset";

	std::unique_ptr<UTexture2D> SourceTexture(CreateTestTexture2D());
	FAssetPackageHeader Header;
	Header.Guid = GenerateAssetGuid();
	Header.Type = "Texture2D";
	Header.AssetPath = "Textures/Test/TestTexture";
	Header.ObjectName = "TestTexture";

	std::string Error;
	if (!UAssetSerializer::Save(Header, *SourceTexture, UAssetPath, &Error))
	{
		std::cerr << "ASSERT FAILED: Save uasset failed: " << Error << '\n';
		return false;
	}

	std::unique_ptr<UStreamableRenderAsset> LoadedAsset(UAssetSerializer::LoadObject(UAssetPath, &Error));
	auto* LoadedTexture = dynamic_cast<UTexture2D*>(LoadedAsset.get());
	if (!AssertTrue(LoadedTexture != nullptr, "LoadObject did not return Texture2D: " + Error))
	{
		return false;
	}
	if (!AssertTrue(!LoadedTexture->HasResidentTextureResource(), "Loaded texture should not have GPU resource yet"))
	{
		return false;
	}

	Dx12Renderer Renderer;
	LoadedTexture->InitResource(&Renderer);
	if (!AssertTrue(
			LoadedTexture->HasResidentTextureResource(),
			"Loaded texture HasResidentTextureResource should be true after InitResource"))
	{
		return false;
	}

	std::filesystem::remove_all(TempDir, ErrorCode);
	return true;
}
} // namespace

int main()
{
	if (!TestTexture2DRoundTrip())
	{
		return 1;
	}
	if (!TestInitResourceCreatesGpuResource())
	{
		return 1;
	}
	if (!TestLoadedTextureInitResourceCreatesGpuResource())
	{
		return 1;
	}
	if (!TestGetOrLoadTextureViaRegistry())
	{
		return 1;
	}
	if (!TestGetOrLoadTextureCacheHit())
	{
		return 1;
	}
	if (!TestGetOrLoadTextureWithoutSourceOrMeta())
	{
		return 1;
	}
	if (!TestGetOrLoadTextureViaContentPath())
	{
		return 1;
	}
	if (!TestGetOrLoadTextureFailureCases())
	{
		return 1;
	}

	std::cout << "TextureSerializerTests passed.\n";
	return 0;
}
