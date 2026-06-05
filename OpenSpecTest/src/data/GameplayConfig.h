#pragma once

#include <string>

#include <nlohmann/json.hpp>

struct GameplayConfig
{
	int version = 0;
	std::string map_id;
	float spawn_x = 0.0f;
	float spawn_y = 0.0f;
	float spawn_z = 0.0f;
	int starting_coins = 0;
};

inline void to_json(nlohmann::json& OutJson, const GameplayConfig& InConfig)
{
	OutJson = nlohmann::json{
		{"version", InConfig.version},
		{"map_id", InConfig.map_id},
		{"spawn_x", InConfig.spawn_x},
		{"spawn_y", InConfig.spawn_y},
		{"spawn_z", InConfig.spawn_z},
		{"starting_coins", InConfig.starting_coins}
	};
}

inline void from_json(const nlohmann::json& InJson, GameplayConfig& OutConfig)
{
	if (InJson.contains("version"))
	{
		InJson.at("version").get_to(OutConfig.version);
	}
	if (InJson.contains("map_id"))
	{
		InJson.at("map_id").get_to(OutConfig.map_id);
	}
	if (InJson.contains("spawn_x"))
	{
		InJson.at("spawn_x").get_to(OutConfig.spawn_x);
	}
	if (InJson.contains("spawn_y"))
	{
		InJson.at("spawn_y").get_to(OutConfig.spawn_y);
	}
	if (InJson.contains("spawn_z"))
	{
		InJson.at("spawn_z").get_to(OutConfig.spawn_z);
	}
	if (InJson.contains("starting_coins"))
	{
		InJson.at("starting_coins").get_to(OutConfig.starting_coins);
	}
}
