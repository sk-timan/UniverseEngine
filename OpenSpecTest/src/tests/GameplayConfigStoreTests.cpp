#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "data/GameplayConfigStore.h"

namespace {
bool WriteTextFile(const std::filesystem::path& InPath, const std::string& InText)
{
	std::ofstream Out(InPath, std::ios::out | std::ios::trunc | std::ios::binary);
	if (!Out.is_open())
	{
		return false;
	}
	Out << InText;
	return Out.good();
}

bool AssertTrue(bool bCondition, const std::string& InMessage)
{
	if (!bCondition)
	{
		std::cerr << "ASSERT FAILED: " << InMessage << '\n';
		return false;
	}
	return true;
}
} // namespace

int main()
{
	const auto TempRoot = std::filesystem::temp_directory_path() / "openspec_gameplay_config_store_tests";
	std::error_code Ec;
	std::filesystem::remove_all(TempRoot, Ec);
	std::filesystem::create_directories(TempRoot, Ec);
	if (Ec)
	{
		std::cerr << "Failed to create temp directory: " << TempRoot.string() << '\n';
		return 1;
	}

	const std::filesystem::path ConfigPath = TempRoot / "gameplay.json";
	GameplayConfig Config{};
	Config.version = 1;
	Config.map_id = "test_map";
	Config.spawn_x = 1.0f;
	Config.spawn_y = 2.0f;
	Config.spawn_z = -3.0f;
	Config.starting_coins = 120;

	std::string Error;
	if (!AssertTrue(GameplayConfigStore::SaveToFile(ConfigPath, Config, &Error), "SaveToFile should succeed for valid config. Error: " + Error))
	{
		return 1;
	}

	GameplayConfig Loaded{};
	Error.clear();
	if (!AssertTrue(GameplayConfigStore::LoadFromFile(ConfigPath, &Loaded, &Error), "LoadFromFile should succeed for saved config. Error: " + Error))
	{
		return 1;
	}
	if (!AssertTrue(Loaded.version == Config.version, "version mismatch"))
	{
		return 1;
	}
	if (!AssertTrue(Loaded.map_id == Config.map_id, "map_id mismatch"))
	{
		return 1;
	}
	if (!AssertTrue(Loaded.starting_coins == Config.starting_coins, "starting_coins mismatch"))
	{
		return 1;
	}

	const std::string LegacyJson = R"({
  "version": 2,
  "map_id": "legacy_map",
  "spawn": [4.0, 5.0, 6.0],
  "starting_coins": 200
})";
	if (!AssertTrue(WriteTextFile(ConfigPath, LegacyJson), "Failed to write legacy json fixture"))
	{
		return 1;
	}

	GameplayConfig LegacyLoaded{};
	Error.clear();
	if (!AssertTrue(GameplayConfigStore::LoadFromFile(ConfigPath, &LegacyLoaded, &Error), "LoadFromFile should support legacy schema. Error: " + Error))
	{
		return 1;
	}
	if (!AssertTrue(LegacyLoaded.map_id == "legacy_map", "legacy map_id mismatch"))
	{
		return 1;
	}
	if (!AssertTrue(LegacyLoaded.starting_coins == 200, "legacy starting_coins mismatch"))
	{
		return 1;
	}

	GameplayConfig Invalid{};
	Invalid.version = 1;
	Invalid.map_id = "";
	Invalid.starting_coins = 1;
	Error.clear();
	if (!AssertTrue(!GameplayConfigStore::Validate(Invalid, &Error), "Validate should fail when map_id is empty"))
	{
		return 1;
	}

	std::filesystem::remove_all(TempRoot, Ec);
	return 0;
}
