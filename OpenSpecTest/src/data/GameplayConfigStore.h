#pragma once

#include <filesystem>
#include <string>

#include <nlohmann/json_fwd.hpp>

#include "data/GameplayConfig.h"

class GameplayConfigStore
{
public:
	static bool LoadFromFile(const std::filesystem::path& InFilePath, GameplayConfig* OutConfig, std::string* OutErrorMessage);

	static bool SaveToFile(const std::filesystem::path& InFilePath, const GameplayConfig& InConfig, std::string* OutErrorMessage);

	static bool Validate(const GameplayConfig& InConfig, std::string* OutErrorMessage);

private:
	static bool LoadFromJsonObject(const std::filesystem::path& InFilePath, const nlohmann::json& InRoot, GameplayConfig* OutConfig, std::string* OutErrorMessage);
};
