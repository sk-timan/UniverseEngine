#pragma once

#include <string>

#include "world/Actor.h"

struct FActorSpawnParams;
class ULevel;
class USkeletalMesh;
class USkeletalMeshComponent;

class ASkeletalMeshActor : public AActor
{
public:
	ASkeletalMeshActor(uint64_t InObjectId, std::string InObjectName, const UClass* InClass = nullptr);
	~ASkeletalMeshActor() override = default;

	static const UClass& StaticClass();

	static ASkeletalMeshActor* Spawn(
		ULevel* InLevel,
		const FActorSpawnParams& InParams,
		USkeletalMesh* InSkeletalMesh,
		std::string* OutErrorMessage);

	USkeletalMeshComponent* GetSkeletalMeshComponent() const;
};
