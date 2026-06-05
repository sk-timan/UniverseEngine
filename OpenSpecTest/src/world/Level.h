#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/ObjectRegistry.h"
#include "core/UClass.h"
#include "data/GameplayConfig.h"
#include "world/Actor.h"
#include "world/ActorTransform.h"
#include "world/LevelSaveData.h"

enum class ELevelState : uint8_t
{
	Unloaded = 0,
	Loading,
	Loaded,
	Unloading,
};

struct FActorSpawnParams
{
	std::string Name;
	FActorTransform ActorTransform{};
	bool bAddToRoot = false;
};

class UStaticMeshComponent;
class USkeletalMeshComponent;

class ULevel
{
public:
	ULevel(ObjectRegistry* InObjectRegistry, std::string InLevelId, std::filesystem::path InMapModelPath);

	const std::string& GetLevelId() const;
	ELevelState GetState() const;
	const std::filesystem::path& GetMapModelPath() const;
	size_t GetActorCount() const;
	const std::vector<uint64_t>& GetActorObjectIds() const;

	bool Load(std::string* OutErrorMessage);
	bool Unload(std::string* OutErrorMessage);
	void Tick(float InDeltaSeconds);

	void Render(class IRendererInterface* InRenderer);

	AActor* SpawnActor(const FActorSpawnParams& InParams, std::string* OutErrorMessage);
	AActor* SpawnActorOfClass(const UClass& InActorClass, const FActorSpawnParams& InParams, std::string* OutErrorMessage);
	bool ImportModelFromFile(
		const std::filesystem::path& InFilePath,
		const FActorTransform& InActorTransform,
		AActor** OutActor,
		std::string* OutErrorMessage);
	bool DestroyActor(uint64_t InActorObjectId);
	AActor* FindActor(uint64_t InActorObjectId);
	const AActor* FindActor(uint64_t InActorObjectId) const;

	FLevelSaveData GetSaveData() const;
	bool SaveToFile(const std::filesystem::path& InFilePath, std::string* OutErrorMessage);
	bool LoadFromFile(const std::filesystem::path& InFilePath, std::string* OutErrorMessage);

	const GameplayConfig& GetGameplayConfig() const;
	void SetGameplayConfig(const GameplayConfig& InConfig);

	ObjectRegistry* GetObjectRegistry();
	const ObjectRegistry* GetObjectRegistry() const;

private:
	UStaticMeshComponent* ResolveStaticMeshComponentAsset(UActorComponent* InComponent, std::string* OutErrorMessage);
	USkeletalMeshComponent* ResolveSkeletalMeshComponentAsset(UActorComponent* InComponent, std::string* OutErrorMessage);
	std::filesystem::path ResolveAssetImportPath(const std::string& InAssetId) const;

	ObjectRegistry* ObjectRegistry_ = nullptr;
	std::string LevelId_;
	ELevelState State_ = ELevelState::Unloaded;
	std::filesystem::path MapModelPath_{};
	GameplayConfig GameplayConfig_{};
	uint64_t LevelRootObjectId_ = 0;
	std::vector<uint64_t> ActorObjectIds_{};
};
