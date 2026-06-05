#pragma once

#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>

#include "data/GameplayConfig.h"

class UStaticMesh;
class USkeletalMesh;
class UStreamableRenderAsset;

class ResourceLoader
{
public:
	struct ModelAsset
	{
		std::filesystem::path source_path;
		uint32_t mesh_count = 0;
		uint32_t node_count = 0;
		uint32_t material_count = 0;
		uint32_t total_vertex_count = 0;
		uint32_t total_index_count = 0;
	};

	struct FishSpeciesDef
	{
		std::string id;
		std::string name;
		float min_weight_kg = 0.0f;
		float max_weight_kg = 0.0f;
		std::string power_curve;
	};

	bool Initialize(const std::filesystem::path& InDataRoot, std::string* OutErrorMessage);

	const std::filesystem::path& data_root() const { return m_data_root_; }

	bool LoadJsonText(const std::filesystem::path& InRelativePath, std::string* OutText, std::string* OutErrorMessage) const;

	bool LoadCsvRows(const std::filesystem::path& InRelativePath, std::vector<std::string>* OutRows, std::string* OutErrorMessage) const;

	bool LoadModelAsset(const std::filesystem::path& InRelativePath, ModelAsset* OutModelAsset, std::string* OutErrorMessage) const;

	bool LoadGameplayConfig(const std::filesystem::path& InRelativePath, GameplayConfig* OutConfig, std::string* OutErrorMessage) const;

	bool LoadFishSpeciesDefs(const std::filesystem::path& InRelativePath, std::vector<FishSpeciesDef>* OutSpeciesDefs, std::string* OutErrorMessage) const;

	UStaticMesh* LoadStaticMesh(const std::filesystem::path& InRelativePath, std::string* OutErrorMessage);
	USkeletalMesh* LoadSkeletalMesh(const std::filesystem::path& InRelativePath, std::string* OutErrorMessage);

	void RegisterLoadedAsset(UStreamableRenderAsset* InAsset);

private:
	static bool ReadTextFile(const std::filesystem::path& InPath, std::string* OutText, std::string* OutErrorMessage);
	static std::string TrimLeadingAsciiWhitespace(const std::string& InValue);
	static std::string TrimAsciiWhitespace(const std::string& InValue);
	static std::vector<std::string> SplitCsvLine(const std::string& InLine);
	static bool ParseFloat(const std::string& InText, float* OutValue);
	static std::string BuildMissingPathMessage(const std::filesystem::path& InFullPath);

	std::filesystem::path m_data_root_{};
};
