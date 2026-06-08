#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "asset/AssetPackageHeader.h"

struct FAssetRegistryEntry
{
	std::string AssetPath;
	std::string ObjectName;
	std::string Type;
	std::string Guid;
	std::filesystem::path UAssetFilePath;
	std::string SourceFile;
	std::vector<std::string> DependsOn;
};

class FAssetRegistry
{
public:
	static FAssetRegistry& Get();

	void ScanContentDirectory();
	void RegisterFromDisk(const std::filesystem::path& InUAssetPath);
	void RegisterFromHeader(const FAssetPackageHeader& InHeader, const std::filesystem::path& InUAssetPath);

	std::optional<FAssetRegistryEntry> FindBySoftPath(const std::string& InSoftPath) const;
	std::optional<FAssetRegistryEntry> FindByAssetPath(const std::string& InAssetPath) const;
	std::vector<FAssetRegistryEntry> ListAssets(const std::string& InTypeFilter = {}) const;
	uint64_t GetRevision() const;

private:
	FAssetRegistry() = default;

	void RegisterScannedUAsset(const std::filesystem::path& InUAssetPath);
	void BumpRevision();

	std::vector<FAssetRegistryEntry> Entries_;
	uint64_t Revision_ = 0;
};
