#include "asset/ProjectPaths.h"

#include "asset/AssetSerializer.h"
#include "asset/AssetPackageHeader.h"
#include "asset/SoftObjectPath.h"

namespace
{
std::filesystem::path GetOpenSpecTestRoot()
{
	return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}
} // namespace

const std::filesystem::path GProjectDataDirectory = GetOpenSpecTestRoot() / "data";
const std::filesystem::path GProjectContentDirectory = GetOpenSpecTestRoot() / "Content";

std::filesystem::path ResolveContentFilePath(const std::filesystem::path& InRelativePath)
{
	if (InRelativePath.empty())
	{
		return {};
	}
	if (InRelativePath.is_absolute())
	{
		return InRelativePath;
	}
	return GProjectContentDirectory / InRelativePath;
}

bool TryBuildSoftPathFromUAssetFile(
	const std::filesystem::path& InUAssetFilePath,
	std::string* OutSoftObjectPath,
	std::string* OutErrorMessage)
{
	if (OutSoftObjectPath == nullptr)
	{
		return false;
	}
	OutSoftObjectPath->clear();

	if (InUAssetFilePath.empty() || InUAssetFilePath.extension() != ".uasset")
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Invalid uasset file path: " + InUAssetFilePath.string();
		}
		return false;
	}

	std::error_code ErrorCode;
	const std::filesystem::path RelativePath =
		std::filesystem::relative(InUAssetFilePath, GProjectContentDirectory, ErrorCode);
	if (!ErrorCode && !RelativePath.empty())
	{
		std::filesystem::path AssetPathPath = RelativePath;
		AssetPathPath.replace_extension("");
		const std::string AssetPath = AssetPathPath.generic_string();
		const std::string ObjectName = AssetPathPath.filename().string();
		*OutSoftObjectPath = FSoftObjectPath::Build(AssetPath, ObjectName);
		return true;
	}

	FAssetPackageHeader Header;
	if (UAssetSerializer::LoadHeader(InUAssetFilePath, &Header, OutErrorMessage))
	{
		*OutSoftObjectPath = FSoftObjectPath::Build(Header.AssetPath, Header.ObjectName);
		return true;
	}

	if (OutErrorMessage != nullptr && OutErrorMessage->empty())
	{
		*OutErrorMessage = "Failed to resolve SoftObjectPath from uasset: " + InUAssetFilePath.string();
	}
	return false;
}
