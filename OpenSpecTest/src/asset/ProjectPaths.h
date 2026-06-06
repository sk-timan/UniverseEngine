#pragma once

#include <filesystem>

extern const std::filesystem::path GProjectDataDirectory;
extern const std::filesystem::path GProjectContentDirectory;

std::filesystem::path ResolveContentFilePath(const std::filesystem::path& InRelativePath);
bool TryBuildSoftPathFromUAssetFile(
	const std::filesystem::path& InUAssetFilePath,
	std::string* OutSoftObjectPath,
	std::string* OutErrorMessage);
