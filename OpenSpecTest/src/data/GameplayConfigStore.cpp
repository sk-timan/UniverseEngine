#include "data/GameplayConfigStore.h"

#include <cmath>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace 
{
bool ReadTextFile(const std::filesystem::path& InPath, std::string* OutText, std::string* OutErrorMessage)
{
	if (OutText == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "GameplayConfigStore: out_text is null.";
		}
		return false;
	}

	std::ifstream File(InPath, std::ios::in | std::ios::binary);
	if (!File.is_open())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to open gameplay config file: " + InPath.string();
		}
		return false;
	}

	std::ostringstream Buffer;
	Buffer << File.rdbuf();
	*OutText = Buffer.str();
	return true;
}

bool IsFinite(float InValue)
{
	return std::isfinite(InValue);
}
} // namespace

bool GameplayConfigStore::LoadFromFile(const std::filesystem::path& InFilePath, GameplayConfig* OutConfig, std::string* OutErrorMessage)
{
	if (OutConfig == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "GameplayConfigStore: out_config is null.";
		}
		return false;
	}

	std::string Text;
	if (!ReadTextFile(InFilePath, &Text, OutErrorMessage))
	{
		return false;
	}

	const nlohmann::json Root = nlohmann::json::parse(Text, nullptr, false);
	if (Root.is_discarded() || !Root.is_object())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Invalid JSON object in gameplay config: " + InFilePath.string();
		}
		return false;
	}

	return LoadFromJsonObject(InFilePath, Root, OutConfig, OutErrorMessage);
}

bool GameplayConfigStore::SaveToFile(const std::filesystem::path& InFilePath, const GameplayConfig& InConfig, std::string* OutErrorMessage)
{
	if (!Validate(InConfig, OutErrorMessage))
	{
		return false;
	}

	nlohmann::json Root;
	Root["version"] = InConfig.version;
	Root["world"] = {
		{"map_id", InConfig.map_id},
		{"spawn", {InConfig.spawn_x, InConfig.spawn_y, InConfig.spawn_z}},
	};
	Root["economy"] = {
		{"starting_coins", InConfig.starting_coins},
	};

	std::ofstream File(InFilePath, std::ios::out | std::ios::trunc | std::ios::binary);
	if (!File.is_open())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to open gameplay config for writing: " + InFilePath.string();
		}
		return false;
	}
	File << Root.dump(2) << '\n';
	if (!File.good())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to write gameplay config file: " + InFilePath.string();
		}
		return false;
	}

	return true;
}

bool GameplayConfigStore::Validate(const GameplayConfig& InConfig, std::string* OutErrorMessage)
{
	if (InConfig.version <= 0)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Field 'version' must be greater than 0.";
		}
		return false;
	}
	if (InConfig.map_id.empty())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Field 'world.map_id' must not be empty.";
		}
		return false;
	}
	if (!IsFinite(InConfig.spawn_x) || !IsFinite(InConfig.spawn_y) || !IsFinite(InConfig.spawn_z))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Field 'world.spawn' must contain finite numbers.";
		}
		return false;
	}
	if (InConfig.starting_coins < 0)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Field 'economy.starting_coins' must be >= 0.";
		}
		return false;
	}
	return true;
}

bool GameplayConfigStore::LoadFromJsonObject(const std::filesystem::path& InFilePath, const nlohmann::json& InRoot, GameplayConfig* OutConfig, std::string* OutErrorMessage)
{
	GameplayConfig Config{};
	if (!InRoot.contains("version") || !InRoot["version"].is_number_integer())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage =
				"Missing or invalid integer field 'version' in: " + InFilePath.string();
		}
		return false;
	}
	Config.version = InRoot["version"].get<int>();

	bool bHasNestedWorld = false;
	bool bHasNestedEconomy = false;
	if (InRoot.contains("world") && InRoot["world"].is_object())
	{
		const auto& World = InRoot["world"];
		if (World.contains("map_id") && World["map_id"].is_string() && World.contains("spawn") &&
			World["spawn"].is_array() && World["spawn"].size() >= 3 &&
			World["spawn"][0].is_number() && World["spawn"][1].is_number() &&
			World["spawn"][2].is_number())
		{
			Config.map_id = World["map_id"].get<std::string>();
			Config.spawn_x = World["spawn"][0].get<float>();
			Config.spawn_y = World["spawn"][1].get<float>();
			Config.spawn_z = World["spawn"][2].get<float>();
			bHasNestedWorld = true;
		}
	}
	if (InRoot.contains("economy") && InRoot["economy"].is_object())
	{
		const auto& Economy = InRoot["economy"];
		if (Economy.contains("starting_coins") && Economy["starting_coins"].is_number_integer())
		{
			Config.starting_coins = Economy["starting_coins"].get<int>();
			bHasNestedEconomy = true;
		}
	}

	// Backward-compatible fallback for legacy flat schema.
	if (!bHasNestedWorld)
	{
		if (!InRoot.contains("map_id") || !InRoot["map_id"].is_string() ||
			!InRoot.contains("spawn") || !InRoot["spawn"].is_array() ||
			InRoot["spawn"].size() < 3 || !InRoot["spawn"][0].is_number() ||
			!InRoot["spawn"][1].is_number() || !InRoot["spawn"][2].is_number())
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage =
					"Missing world fields. Expected nested world.map_id/world.spawn or legacy map_id/spawn in: " +
					InFilePath.string();
			}
			return false;
		}
		Config.map_id = InRoot["map_id"].get<std::string>();
		Config.spawn_x = InRoot["spawn"][0].get<float>();
		Config.spawn_y = InRoot["spawn"][1].get<float>();
		Config.spawn_z = InRoot["spawn"][2].get<float>();
	}

	if (!bHasNestedEconomy)
	{
		if (!InRoot.contains("starting_coins") || !InRoot["starting_coins"].is_number_integer())
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage =
					"Missing economy field. Expected nested economy.starting_coins or legacy starting_coins in: " +
					InFilePath.string();
			}
			return false;
		}
		Config.starting_coins = InRoot["starting_coins"].get<int>();
	}

	if (!Validate(Config, OutErrorMessage))
	{
		return false;
	}
	*OutConfig = Config;
	return true;
}
