#pragma once

#include <string>

#include "world/Actor.h"

struct FActorSpawnParams;
class ULevel;
class UStaticMesh;
class UStaticMeshComponent;

class AStaticMeshActor : public AActor
{
public:
	AStaticMeshActor(uint64_t InObjectId, std::string InObjectName, const UClass* InClass = nullptr);
	~AStaticMeshActor() override = default;

	static const UClass& StaticClass();

	static AStaticMeshActor* Spawn(
		ULevel* InLevel,
		const FActorSpawnParams& InParams,
		UStaticMesh* InStaticMesh,
		std::string* OutErrorMessage);

	UStaticMeshComponent* GetStaticMeshComponent() const;
};
