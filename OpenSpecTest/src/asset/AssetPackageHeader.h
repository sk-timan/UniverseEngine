#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

struct FAssetPackageHeader
{
	static constexpr const char* Magic = "UEAS";
	static constexpr int Version = 1;

	std::string Guid;
	std::string Type;
	std::string AssetPath;
	std::string ObjectName;
	std::vector<std::string> DependsOn;

	void ToJson(nlohmann::json* OutJson) const;
	bool FromJson(const nlohmann::json& InJson, std::string* OutErrorMessage);
};

struct FAssetMeta
{
	std::string SourceFile;
	std::string SourceTimestamp;
	nlohmann::json ImportSettings = nlohmann::json::object();
	std::string ImportHash;

	void ToJson(nlohmann::json* OutJson) const;
	bool FromJson(const nlohmann::json& InJson, std::string* OutErrorMessage);
};

std::string GenerateAssetGuid();
std::string ComputeImportHash(const std::string& InSourceFile, const std::string& InSourceTimestamp);
std::string GetFileTimestampIso(const std::filesystem::path& InPath);
