#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include "core/ObjectRegistry.h"
#include "data/GameplayConfig.h"
#include "world/Level.h"

struct FLevelDefinition
{
	std::string LevelId;
	std::filesystem::path MapModelPath;
	std::filesystem::path SaveDataPath;
};

class IRendererInterface
{
public:
	virtual ~IRendererInterface() = default;
	virtual void Render(const void* InRenderCommands, size_t InCommandCount) = 0;
};

class UWorld
{
public:
	explicit UWorld(ObjectRegistry* InObjectRegistry);

	bool RegisterLevelDefinition(const FLevelDefinition& InDefinition, std::string* OutErrorMessage);
	bool RegisterLevelDefinitionWithSavePath(const FLevelDefinition& InDefinition, const std::filesystem::path& InSaveDataPath, std::string* OutErrorMessage);
	bool HasLevelDefinition(const std::string& InLevelId) const;
	size_t GetRegisteredLevelCount() const;
	std::filesystem::path GetLevelSavePath(const std::string& InLevelId) const;

	bool LoadLevel(const std::string& InLevelId, std::string* OutErrorMessage);
	bool UnloadLevel(std::string* OutErrorMessage);
	void Tick(float InDeltaSeconds);

	void Render(IRendererInterface* InRenderer);

	ULevel* GetActiveLevel();
	const ULevel* GetActiveLevel() const;
	bool HasActiveLevel() const;
	const std::filesystem::path* GetActiveMapModelPath() const;
	const class GameplayConfig* GetActiveLevelGameplayConfig() const;

private:
	ObjectRegistry* ObjectRegistry_ = nullptr;
	std::unordered_map<std::string, FLevelDefinition> LevelDefinitions_{};
	std::unique_ptr<ULevel> ActiveLevel_;
};
