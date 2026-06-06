#include "asset/AssetManager.h"



#include "asset/AssetRegistry.h"

#include "asset/AssetSerializer.h"

#include "asset/ProjectPaths.h"

#include "asset/SoftObjectPath.h"

#include "render/ResourceRegistry.h"

#include "render/asset/StreamableRenderAsset.h"



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



	const FSoftObjectPath SoftPath = FSoftObjectPath::Parse(InSoftObjectPath);

	if (UStreamableRenderAsset* Existing = ResourceRegistry::Get().FindAssetByReference(InSoftObjectPath))

	{

		if (Existing->HasResidentGeometryData())

		{

			return Existing;

		}

	}



	std::filesystem::path UAssetPath;

	if (const auto RegistryEntry = FAssetRegistry::Get().FindBySoftPath(InSoftObjectPath))

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

