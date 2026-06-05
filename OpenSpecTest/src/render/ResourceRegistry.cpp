#include "render/ResourceRegistry.h"

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
	const std::string AssetId = InAsset->GetAssetPath().string();
	Assets_[AssetId] = InAsset;
}

void ResourceRegistry::UnregisterAsset(const std::string& InAssetId)
{
	auto It = Assets_.find(InAssetId);
	if (It != Assets_.end())
	{
		Assets_.erase(It);
	}
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
