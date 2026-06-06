#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include "render/asset/StreamableRenderAsset.h"

class ResourceRegistry
{
public:
	static ResourceRegistry& Get();

	void RegisterAsset(UStreamableRenderAsset* InAsset);
	void UnregisterAsset(const std::string& InAssetId);
	void UnregisterAssetByPath(const std::filesystem::path& InAssetPath);

	UStreamableRenderAsset* FindAsset(const std::string& InAssetId);
	const UStreamableRenderAsset* FindAsset(const std::string& InAssetId) const;

	// Accepts SoftObjectPath (Meshes/Foo.Bar) or asset path (Meshes/Foo).
	UStreamableRenderAsset* FindAssetByReference(const std::string& InReference);

	template<typename T>
	T* FindAsset(const std::string& InAssetId);

	size_t GetAssetCount() const;
	void Clear();

private:
	ResourceRegistry() = default;

	std::unordered_map<std::string, UStreamableRenderAsset*> Assets_;
};

template<typename T>
T* ResourceRegistry::FindAsset(const std::string& InAssetId)
{
	auto It = Assets_.find(InAssetId);
	if (It != Assets_.end())
	{
		return dynamic_cast<T*>(It->second);
	}
	return nullptr;
}
