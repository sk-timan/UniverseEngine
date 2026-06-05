#include <filesystem>
#include <iostream>
#include <string>

#include "core/ObjectRegistry.h"
#include "world/World.h"

namespace
{
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
	ObjectRegistry Registry;
	UWorld World(&Registry);
	std::string Error;

	if (!AssertTrue(
			World.RegisterLevelDefinition(
				FLevelDefinition{"mvp_pond_01", std::filesystem::path("data/models/FBX/huesitos.fbx")},
				&Error),
			"Register level definition failed: " + Error))
	{
		return 1;
	}
	if (!AssertTrue(World.GetRegisteredLevelCount() == 1, "Expected one registered level"))
	{
		return 1;
	}

	if (!AssertTrue(World.LoadLevel("mvp_pond_01", &Error), "LoadLevel should succeed: " + Error))
	{
		return 1;
	}
	if (!AssertTrue(World.HasActiveLevel(), "World should have active level after load"))
	{
		return 1;
	}
	const ULevel* Active = World.GetActiveLevel();
	if (!AssertTrue(Active != nullptr, "Active level pointer should be valid"))
	{
		return 1;
	}
	if (!AssertTrue(Active->GetState() == ELevelState::Loaded, "Active level should be in Loaded state"))
	{
		return 1;
	}
	if (!AssertTrue(Active->GetActorCount() >= 1, "Loaded level should include at least one actor"))
	{
		return 1;
	}

	if (!AssertTrue(!World.LoadLevel("unknown_map", &Error), "Unknown map must fail to load"))
	{
		return 1;
	}
	if (!AssertTrue(
			Error.find("No level definition found for map_id") != std::string::npos,
			"Unknown map error should include diagnosis"))
	{
		return 1;
	}

	if (!AssertTrue(World.UnloadLevel(&Error), "UnloadLevel should succeed: " + Error))
	{
		return 1;
	}
	if (!AssertTrue(!World.HasActiveLevel(), "World should not keep active level after unload"))
	{
		return 1;
	}
	if (!AssertTrue(Registry.GetObjectCount() == 0, "Registry should be empty after unload and GC"))
	{
		return 1;
	}

	return 0;
}
