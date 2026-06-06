#include "render/ResourceRegistry.h"

#include "asset/SoftObjectPath.h"

ResourceRegistry& ResourceRegistry::Get()
{
	static ResourceRegistry Instance;
	return Instance;
}

void ResourceRegistry::RegisterAsset(UStreamableRenderAsset* InAsset)
{
	if (InAsset == nullptr)
	{
		return;
	}
	const std::string AssetPath = InAsset->GetAssetPath().string();
	const std::string SoftPath = FSoftObjectPath::Build(AssetPath, InAsset->GetObjectName());
	Assets_[AssetPath] = InAsset;
	if (!SoftPath.empty() && SoftPath != AssetPath)
	{
		Assets_[SoftPath] = InAsset;
	}
}

void ResourceRegistry::UnregisterAsset(const std::string& InAssetId)
{
	UnregisterAssetByPath(std::filesystem::path(InAssetId));
}

void ResourceRegistry::UnregisterAssetByPath(const std::filesystem::path& InAssetPath)
{
	const std::string Key = InAssetPath.string();
	auto It = Assets_.find(Key);
	if (It != Assets_.end())
	{
		Assets_.erase(It);
	}

	const FSoftObjectPath SoftPath = FSoftObjectPath::Parse(Key);
	const std::string SoftKey = SoftPath.ToString();
	if (SoftKey != Key)
	{
		It = Assets_.find(SoftKey);
		if (It != Assets_.end())
		{
			Assets_.erase(It);
		}
	}
	if (!SoftPath.AssetPath.empty() && SoftPath.AssetPath != Key)
	{
		It = Assets_.find(SoftPath.AssetPath);
		if (It != Assets_.end())
		{
			Assets_.erase(It);
		}
	}
}

UStreamableRenderAsset* ResourceRegistry::FindAssetByReference(const std::string& InReference)
{
	if (InReference.empty())
	{
		return nullptr;
	}

	if (UStreamableRenderAsset* Direct = FindAsset(InReference))
	{
		return Direct;
	}

	const FSoftObjectPath SoftPath = FSoftObjectPath::Parse(InReference);
	const std::string SoftKey = SoftPath.ToString();
	if (!SoftKey.empty() && SoftKey != InReference)
	{
		if (UStreamableRenderAsset* BySoftKey = FindAsset(SoftKey))
		{
			return BySoftKey;
		}
	}
	if (!SoftPath.AssetPath.empty() && SoftPath.AssetPath != InReference && SoftPath.AssetPath != SoftKey)
	{
		return FindAsset(SoftPath.AssetPath);
	}
	return nullptr;
}

UStreamableRenderAsset* ResourceRegistry::FindAsset(const std::string& InAssetId)
{
	auto It = Assets_.find(InAssetId);
	if (It != Assets_.end())
	{
		return It->second;
	}
	return nullptr;
}

const UStreamableRenderAsset* ResourceRegistry::FindAsset(const std::string& InAssetId) const
{
	auto It = Assets_.find(InAssetId);
	if (It != Assets_.end())
	{
		return It->second;
	}
	return nullptr;
}

size_t ResourceRegistry::GetAssetCount() const
{
	return Assets_.size();
}

void ResourceRegistry::Clear()
{
	Assets_.clear();
}
