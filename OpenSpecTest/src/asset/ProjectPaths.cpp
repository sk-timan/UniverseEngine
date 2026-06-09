#include "asset/ProjectPaths.h"

#include <algorithm>

#include "asset/AssetSerializer.h"
#include "asset/AssetPackageHeader.h"
#include "asset/SoftObjectPath.h"

namespace
{
std::filesystem::path GetOpenSpecTestRoot()
{
	return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

std::string U8StringToStdString(const std::u8string& InU8Text)
{
	return std::string(reinterpret_cast<const char*>(InU8Text.data()), InU8Text.size());
}

std::u8string StdStringToU8String(const std::string& InUtf8Text)
{
	return std::u8string(reinterpret_cast<const char8_t*>(InUtf8Text.data()), InUtf8Text.size());
}
} // namespace

const std::filesystem::path GProjectDataDirectory = GetOpenSpecTestRoot() / "data";
const std::filesystem::path GProjectContentDirectory = GetOpenSpecTestRoot() / "Content";

std::string FsPathComponentUtf8(const std::filesystem::path& InPath)
{
	return U8StringToStdString(InPath.filename().u8string());
}

std::string FsPathUtf8Generic(const std::filesystem::path& InPath)
{
	std::string Result = U8StringToStdString(InPath.u8string());
	std::replace(Result.begin(), Result.end(), '\\', '/');
	return Result;
}

std::filesystem::path Utf8GenericToFsPath(const std::string& InUtf8Path)
{
	return std::filesystem::path(StdStringToU8String(InUtf8Path));
}

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
		const std::string AssetPath = FsPathUtf8Generic(AssetPathPath);
		const std::string ObjectName = FsPathComponentUtf8(AssetPathPath);
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
