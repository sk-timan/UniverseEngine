#pragma once

#include <nlohmann/json.hpp>

#include "core/ObjectRegistry.h"
#include "components/ActorComponent.h"
#include "world/ActorSaveData.h"

class AActor;
class USceneComponent;
class UMeshComponent;

class ComponentSerializer
{
public:
	static FComponentSaveData SerializeComponent(const UActorComponent* InComponent);
	static UActorComponent* DeserializeComponent(
		const FComponentSaveData& InData,
		ObjectRegistry* InObjectRegistry,
		AActor* InOwner,
		std::string* OutErrorMessage);

private:
	static void SerializeSceneComponent(const USceneComponent* InComponent, FComponentSaveData* OutData);
	static void SerializeMeshComponent(const UMeshComponent* InComponent, FComponentSaveData* OutData);
};
