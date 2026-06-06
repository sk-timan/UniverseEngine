#pragma once

#include <filesystem>
#include <string>

struct FSoftObjectPath
{
	std::string AssetPath;
	std::string ObjectName;

	static FSoftObjectPath Parse(const std::string& InSoftPath);
	static std::string Build(const std::string& InAssetPath, const std::string& InObjectName);

	std::string ToString() const;

	std::filesystem::path ToUAssetRelativePath() const;
};
