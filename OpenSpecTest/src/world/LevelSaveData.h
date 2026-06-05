#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "data/GameplayConfig.h"
#include "world/ActorSaveData.h"

struct FLevelSaveData
{
	std::string LevelId;
	std::string LevelName;
	std::filesystem::path MapModelPath;
	GameplayConfig GameplayConfig;
	std::vector<FActorSaveData> Actors;
	std::string CreatedAt;
	std::string ModifiedAt;
	int Version = 1;
};

inline void to_json(nlohmann::json& OutJson, const FLevelSaveData& InData)
{
	OutJson = nlohmann::json{
		{"level_id", InData.LevelId},
		{"level_name", InData.LevelName},
		{"map_model_path", InData.MapModelPath.string()},
		{"gameplay_config", InData.GameplayConfig},
		{"actors", InData.Actors},
		{"created_at", InData.CreatedAt},
		{"modified_at", InData.ModifiedAt},
		{"version", InData.Version}
	};
}

inline void from_json(const nlohmann::json& InJson, FLevelSaveData& OutData)
{
	if (InJson.contains("level_id"))
	{
		InJson.at("level_id").get_to(OutData.LevelId);
	}
	if (InJson.contains("level_name"))
	{
		InJson.at("level_name").get_to(OutData.LevelName);
	}
	if (InJson.contains("map_model_path"))
	{
		std::string PathStr;
		InJson.at("map_model_path").get_to(PathStr);
		OutData.MapModelPath = std::filesystem::path(PathStr);
	}
	if (InJson.contains("gameplay_config"))
	{
		InJson.at("gameplay_config").get_to(OutData.GameplayConfig);
	}
	if (InJson.contains("actors"))
	{
		InJson.at("actors").get_to(OutData.Actors);
	}
	if (InJson.contains("created_at"))
	{
		InJson.at("created_at").get_to(OutData.CreatedAt);
	}
	if (InJson.contains("modified_at"))
	{
		InJson.at("modified_at").get_to(OutData.ModifiedAt);
	}
	if (InJson.contains("version"))
	{
		InJson.at("version").get_to(OutData.Version);
	}
}
