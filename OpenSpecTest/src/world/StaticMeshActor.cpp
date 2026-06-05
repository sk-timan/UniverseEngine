#include "world/StaticMeshActor.h"

#include <utility>

#include "components/ActorComponent.h"
#include "components/StaticMeshComponent.h"
#include "core/ObjectRegistry.h"
#include "render/asset/StaticMesh.h"
#include "world/Level.h"

namespace
{
std::unique_ptr<AStaticMeshActor> CreateStaticMeshActorInstance(uint64_t InObjectId, std::string InObjectName)
{
	return std::make_unique<AStaticMeshActor>(InObjectId, std::move(InObjectName), &AStaticMeshActor::StaticClass());
}
} // namespace

AStaticMeshActor::AStaticMeshActor(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
	: AActor(InObjectId, std::move(InObjectName), InClass != nullptr ? InClass : &AStaticMeshActor::StaticClass())
{
}

const UClass& AStaticMeshActor::StaticClass()
{
	static const UClass Class("AStaticMeshActor", &AActor::StaticClass(), CreateStaticMeshActorInstance);
	return Class;
}

AStaticMeshActor* AStaticMeshActor::Spawn(
	ULevel* InLevel,
	const FActorSpawnParams& InParams,
	UStaticMesh* InStaticMesh,
	std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}
	if (InLevel == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Level is null.";
		}
		return nullptr;
	}
	if (InStaticMesh == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Static mesh asset is null.";
		}
		return nullptr;
	}

	AActor* SpawnedActor = InLevel->SpawnActorOfClass(AStaticMeshActor::StaticClass(), InParams, OutErrorMessage);
	if (SpawnedActor == nullptr)
	{
		return nullptr;
	}

	AStaticMeshActor* MeshActor = static_cast<AStaticMeshActor*>(SpawnedActor);
	ObjectRegistry* Registry = InLevel->GetObjectRegistry();
	if (Registry == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Level object registry is null.";
		}
		return nullptr;
	}

	std::string RegistryError;
	const std::string ComponentName = InParams.Name + "_StaticMeshComponent";
	UObject* ComponentObject = Registry->NewObject(
		UStaticMeshComponent::StaticClass(),
		FNewObjectParams{ComponentName, false},
		&RegistryError);
	UStaticMeshComponent* MeshComponent = static_cast<UStaticMeshComponent*>(ComponentObject);
	if (MeshComponent == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to create static mesh component: " + RegistryError;
		}
		return nullptr;
	}

	MeshActor->SetRootComponent(MeshComponent);
	MeshComponent->SetStaticMesh(InStaticMesh);
	MeshActor->AddComponent(MeshComponent);
	MeshActor->AddReferencedObject(MeshComponent);
	MeshComponent->SetOuter(MeshActor->GetObjectId());
	MeshActor->AddInner(MeshComponent->GetObjectId());
	MeshActor->SetActorTransform(InParams.ActorTransform);
	MeshActor->ApplyActorTransformToRoot();

	return MeshActor;
}

UStaticMeshComponent* AStaticMeshActor::GetStaticMeshComponent() const
{
	for (UActorComponent* Component : GetComponents())
	{
		if (Component != nullptr && Component->IsA(UStaticMeshComponent::StaticClass()))
		{
			return static_cast<UStaticMeshComponent*>(Component);
		}
	}
	return nullptr;
}
