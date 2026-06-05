#include "world/SkeletalMeshActor.h"

#include <utility>

#include "components/ActorComponent.h"
#include "components/SkeletalMeshComponent.h"
#include "core/ObjectRegistry.h"
#include "render/asset/SkeletalMesh.h"
#include "world/Level.h"

namespace
{
std::unique_ptr<ASkeletalMeshActor> CreateSkeletalMeshActorInstance(uint64_t InObjectId, std::string InObjectName)
{
	return std::make_unique<ASkeletalMeshActor>(InObjectId, std::move(InObjectName), &ASkeletalMeshActor::StaticClass());
}
} // namespace

ASkeletalMeshActor::ASkeletalMeshActor(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
	: AActor(InObjectId, std::move(InObjectName), InClass != nullptr ? InClass : &ASkeletalMeshActor::StaticClass())
{
}

const UClass& ASkeletalMeshActor::StaticClass()
{
	static const UClass Class("ASkeletalMeshActor", &AActor::StaticClass(), CreateSkeletalMeshActorInstance);
	return Class;
}

ASkeletalMeshActor* ASkeletalMeshActor::Spawn(
	ULevel* InLevel,
	const FActorSpawnParams& InParams,
	USkeletalMesh* InSkeletalMesh,
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
	if (InSkeletalMesh == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Skeletal mesh asset is null.";
		}
		return nullptr;
	}

	AActor* SpawnedActor = InLevel->SpawnActorOfClass(ASkeletalMeshActor::StaticClass(), InParams, OutErrorMessage);
	if (SpawnedActor == nullptr)
	{
		return nullptr;
	}

	ASkeletalMeshActor* MeshActor = static_cast<ASkeletalMeshActor*>(SpawnedActor);
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
	const std::string ComponentName = InParams.Name + "_SkeletalMeshComponent";
	UObject* ComponentObject = Registry->NewObject(
		USkeletalMeshComponent::StaticClass(),
		FNewObjectParams{ComponentName, false},
		&RegistryError);
	USkeletalMeshComponent* MeshComponent = static_cast<USkeletalMeshComponent*>(ComponentObject);
	if (MeshComponent == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to create skeletal mesh component: " + RegistryError;
		}
		return nullptr;
	}

	MeshActor->SetRootComponent(MeshComponent);
	MeshComponent->SetSkeletalMesh(InSkeletalMesh);
	MeshActor->AddComponent(MeshComponent);
	MeshActor->AddReferencedObject(MeshComponent);
	MeshComponent->SetOuter(MeshActor->GetObjectId());
	MeshActor->AddInner(MeshComponent->GetObjectId());
	MeshActor->SetActorTransform(InParams.ActorTransform);
	MeshActor->ApplyActorTransformToRoot();

	return MeshActor;
}

USkeletalMeshComponent* ASkeletalMeshActor::GetSkeletalMeshComponent() const
{
	for (UActorComponent* Component : GetComponents())
	{
		if (Component != nullptr && Component->IsA(USkeletalMeshComponent::StaticClass()))
		{
			return static_cast<USkeletalMeshComponent*>(Component);
		}
	}
	return nullptr;
}
