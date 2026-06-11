#include "asset/AssetManager.h"

#include "asset/AssetReferenceResolver.h"
#include "asset/AssetRegistry.h"
#include "asset/AssetSerializer.h"
#include "asset/ProjectPaths.h"
#include "asset/SoftObjectPath.h"
#include "render/ResourceRegistry.h"
#include "render/asset/StreamableRenderAsset.h"
#include "render/asset/Texture.h"



UAssetManager& UAssetManager::Get()

{

	static UAssetManager Instance;

	return Instance;

}



UStreamableRenderAsset* UAssetManager::GetOrLoad(const std::string& InSoftObjectPath, std::string* OutErrorMessage)
{
	if (InSoftObjectPath.empty())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "SoftObjectPath is empty";
		}
		return nullptr;
	}

	const FResolvedAssetReference ResolvedReference =
		FAssetReferenceResolver::Resolve(std::string(), InSoftObjectPath);
	const std::string& EffectiveSoftObjectPath = ResolvedReference.SoftObjectPath.empty()
		? InSoftObjectPath
		: ResolvedReference.SoftObjectPath;

	const FSoftObjectPath SoftPath = FSoftObjectPath::Parse(EffectiveSoftObjectPath);
	if (UStreamableRenderAsset* Existing = ResourceRegistry::Get().FindAssetByReference(EffectiveSoftObjectPath))
	{
		if (Existing->HasResidentGeometryData())
		{
			return Existing;
		}

		if (const UTexture* Texture = dynamic_cast<const UTexture*>(Existing))
		{
			if (Texture->HasResidentPlatformData())
			{
				return Existing;
			}
		}
	}

	std::filesystem::path UAssetPath;
	if (const auto RegistryEntry = FAssetRegistry::Get().FindBySoftPath(EffectiveSoftObjectPath))
	{
		UAssetPath = RegistryEntry->UAssetFilePath;
	}
	else
	{
		UAssetPath = ResolveContentFilePath(SoftPath.ToUAssetRelativePath());
	}

	if (!std::filesystem::exists(UAssetPath))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "uasset not found: " + UAssetPath.string();
		}
		return nullptr;
	}

	UStreamableRenderAsset* LoadedAsset = UAssetSerializer::LoadObject(UAssetPath, OutErrorMessage);
	if (LoadedAsset == nullptr)
	{
		return nullptr;
	}

	LoadedAsset->SetAssetPath(SoftPath.AssetPath);
	LoadedAsset->SetObjectName(SoftPath.ObjectName);
	ResourceRegistry::Get().RegisterAsset(LoadedAsset);
	return LoadedAsset;
}



void UAssetManager::Unload(const std::string& InSoftObjectPath)

{

	ResourceRegistry::Get().UnregisterAssetByPath(std::filesystem::path(InSoftObjectPath));

}



void UAssetManager::ClearLoadedAssets()

{

	ResourceRegistry::Get().Clear();

}

