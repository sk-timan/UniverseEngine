#pragma once

#include <string>

class UStreamableRenderAsset;

class UAssetManager
{
public:
	static UAssetManager& Get();

	UStreamableRenderAsset* GetOrLoad(const std::string& InSoftObjectPath, std::string* OutErrorMessage);
	void Unload(const std::string& InSoftObjectPath);
	void ClearLoadedAssets();

private:
	UAssetManager() = default;
};
