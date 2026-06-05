#include "world/World.h"

UWorld::UWorld(ObjectRegistry* InObjectRegistry)
	: ObjectRegistry_(InObjectRegistry)
{
}

bool UWorld::RegisterLevelDefinition(const FLevelDefinition& InDefinition, std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}
	if (InDefinition.LevelId.empty())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "LevelId must not be empty.";
		}
		return false;
	}
	LevelDefinitions_[InDefinition.LevelId] = InDefinition;
	return true;
}

bool UWorld::RegisterLevelDefinitionWithSavePath(const FLevelDefinition& InDefinition, const std::filesystem::path& InSaveDataPath, std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}
	if (InDefinition.LevelId.empty())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "LevelId must not be empty.";
		}
		return false;
	}
	FLevelDefinition Def = InDefinition;
	Def.SaveDataPath = InSaveDataPath;
	LevelDefinitions_[InDefinition.LevelId] = Def;
	return true;
}

std::filesystem::path UWorld::GetLevelSavePath(const std::string& InLevelId) const
{
	const auto DefIt = LevelDefinitions_.find(InLevelId);
	if (DefIt == LevelDefinitions_.end())
	{
		return std::filesystem::path();
	}
	return DefIt->second.SaveDataPath;
}

bool UWorld::HasLevelDefinition(const std::string& InLevelId) const
{
	return LevelDefinitions_.find(InLevelId) != LevelDefinitions_.end();
}

size_t UWorld::GetRegisteredLevelCount() const
{
	return LevelDefinitions_.size();
}

bool UWorld::LoadLevel(const std::string& InLevelId, std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}
	if (InLevelId.empty())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "InLevelId is empty.";
		}
		return false;
	}

	const auto DefIt = LevelDefinitions_.find(InLevelId);
	if (DefIt == LevelDefinitions_.end())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "No level definition found for map_id: " + InLevelId;
		}
		return false;
	}

	if (ActiveLevel_ != nullptr && ActiveLevel_->GetLevelId() == InLevelId &&
		ActiveLevel_->GetState() == ELevelState::Loaded)
	{
		return true;
	}

	std::string UnloadError;
	if (!UnloadLevel(&UnloadError))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to unload current level before load: " + UnloadError;
		}
		return false;
	}

	std::unique_ptr<ULevel> NewLevel = std::make_unique<ULevel>(ObjectRegistry_, DefIt->second.LevelId, DefIt->second.MapModelPath);

	std::string LoadError;
	if (!NewLevel->Load(&LoadError))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to load level '" + InLevelId + "': " + LoadError;
		}
		return false;
	}

	ActiveLevel_ = std::move(NewLevel);
	return true;
}

bool UWorld::UnloadLevel(std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}
	if (ActiveLevel_ == nullptr)
	{
		return true;
	}

	if (!ActiveLevel_->Unload(OutErrorMessage))
	{
		return false;
	}
	ActiveLevel_.reset();
	return true;
}

void UWorld::Tick(float InDeltaSeconds)
{
	if (ActiveLevel_ == nullptr)
	{
		return;
	}
	ActiveLevel_->Tick(InDeltaSeconds);
}

ULevel* UWorld::GetActiveLevel()
{
	return ActiveLevel_.get();
}

const ULevel* UWorld::GetActiveLevel() const
{
	return ActiveLevel_.get();
}

bool UWorld::HasActiveLevel() const
{
	return ActiveLevel_ != nullptr && ActiveLevel_->GetState() == ELevelState::Loaded;
}

const std::filesystem::path* UWorld::GetActiveMapModelPath() const
{
	if (!HasActiveLevel())
	{
		return nullptr;
	}
	return &ActiveLevel_->GetMapModelPath();
}

const GameplayConfig* UWorld::GetActiveLevelGameplayConfig() const
{
	if (!HasActiveLevel())
	{
		return nullptr;
	}
	return &ActiveLevel_->GetGameplayConfig();
}

void UWorld::Render(IRendererInterface* InRenderer)
{
	if (InRenderer == nullptr || ActiveLevel_ == nullptr)
	{
		return;
	}
	ActiveLevel_->Render(InRenderer);
}
