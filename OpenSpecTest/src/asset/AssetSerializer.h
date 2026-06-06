#pragma once

#include <filesystem>
#include <string>

#include <nlohmann/json_fwd.hpp>

class UStreamableRenderAsset;
class UStaticMesh;
class USkeletalMesh;

struct FAssetPackageHeader;

class UAssetSerializer
{
public:
	static bool Save(
		const FAssetPackageHeader& InHeader,
		const UStreamableRenderAsset& InAsset,
		const std::filesystem::path& InUAssetPath,
		std::string* OutErrorMessage);

	static bool LoadHeader(
		const std::filesystem::path& InUAssetPath,
		FAssetPackageHeader* OutHeader,
		std::string* OutErrorMessage);

	static UStreamableRenderAsset* LoadObject(
		const std::filesystem::path& InUAssetPath,
		std::string* OutErrorMessage);

	static bool SaveMeta(
		const struct FAssetMeta& InMeta,
		const std::filesystem::path& InMetaPath,
		std::string* OutErrorMessage);

	static bool LoadMeta(
		const std::filesystem::path& InMetaPath,
		struct FAssetMeta* OutMeta,
		std::string* OutErrorMessage);

	static std::filesystem::path GetMetaPathForUAsset(const std::filesystem::path& InUAssetPath);
};
