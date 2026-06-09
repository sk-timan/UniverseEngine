#pragma once

#include <filesystem>
#include <string>

extern const std::filesystem::path GProjectDataDirectory;
extern const std::filesystem::path GProjectContentDirectory;

std::filesystem::path ResolveContentFilePath(const std::filesystem::path& InRelativePath);
std::string FsPathComponentUtf8(const std::filesystem::path& InPath);
std::string FsPathUtf8Generic(const std::filesystem::path& InPath);
std::filesystem::path Utf8GenericToFsPath(const std::string& InUtf8Path);
bool TryBuildSoftPathFromUAssetFile(
	const std::filesystem::path& InUAssetFilePath,
	std::string* OutSoftObjectPath,
	std::string* OutErrorMessage);
