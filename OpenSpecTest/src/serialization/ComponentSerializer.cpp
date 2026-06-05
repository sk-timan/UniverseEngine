#include "serialization/ComponentSerializer.h"

#include "components/ActorComponent.h"
#include "components/MeshComponent.h"
#include "world/Actor.h"
#include "components/SceneComponent.h"
#include "components/SkeletalMeshComponent.h"
#include "components/StaticMeshComponent.h"

FComponentSaveData ComponentSerializer::SerializeComponent(const UActorComponent* InComponent)
{
	FComponentSaveData Data;
	if (InComponent == nullptr)
	{
		return Data;
	}

	Data.ComponentId = std::to_string(InComponent->GetObjectId());
	Data.ComponentKey = InComponent->GetObjectName();
	Data.ComponentName = InComponent->GetObjectName();
	Data.ComponentClass = InComponent->GetClass().GetTypeName();

	if (InComponent->IsA(USceneComponent::StaticClass()))
	{
		SerializeSceneComponent(static_cast<const USceneComponent*>(InComponent), &Data);
	}

	if (InComponent->IsA(UMeshComponent::StaticClass()))
	{
		SerializeMeshComponent(static_cast<const UMeshComponent*>(InComponent), &Data);
	}

	return Data;
}

UActorComponent* ComponentSerializer::DeserializeComponent(
	const FComponentSaveData& InData,
	ObjectRegistry* InObjectRegistry,
	AActor* InOwner,
	std::string* OutErrorMessage)
{
	if (InObjectRegistry == nullptr || InOwner == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Object registry or owner actor is null";
		}
		return nullptr;
	}

	std::string ErrorMessage;
	UObject* ComponentObject = nullptr;
	if (InData.ComponentClass == "UStaticMeshComponent")
	{
		ComponentObject = InObjectRegistry->NewObject(
			UStaticMeshComponent::StaticClass(),
			FNewObjectParams{InData.ComponentName, false},
			&ErrorMessage);
	}
	else if (InData.ComponentClass == "USkeletalMeshComponent")
	{
		ComponentObject = InObjectRegistry->NewObject(
			USkeletalMeshComponent::StaticClass(),
			FNewObjectParams{InData.ComponentName, false},
			&ErrorMessage);
	}
	else if (InData.ComponentClass == "UMeshComponent")
	{
		ComponentObject = InObjectRegistry->NewObject(
			UMeshComponent::StaticClass(),
			FNewObjectParams{InData.ComponentName, false},
			&ErrorMessage);
	}
	else if (InData.ComponentClass == "USceneComponent")
	{
		ComponentObject = InObjectRegistry->NewObject(
			USceneComponent::StaticClass(),
			FNewObjectParams{InData.ComponentName, false},
			&ErrorMessage);
	}
	else
	{
		ComponentObject = InObjectRegistry->NewObject(
			UActorComponent::StaticClass(),
			FNewObjectParams{InData.ComponentName, false},
			&ErrorMessage);
	}

	if (ComponentObject == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to create component: " + ErrorMessage;
		}
		return nullptr;
	}

	UActorComponent* Component = static_cast<UActorComponent*>(ComponentObject);
	Component->SetOwnerActor(InOwner);

	if (Component->IsA(USceneComponent::StaticClass()))
	{
		USceneComponent* SceneComponent = static_cast<USceneComponent*>(Component);
		SceneComponent->SetRelativeLocation(InData.RelativeLocation);
		SceneComponent->SetRelativeRotation(
			FRotator3(InData.RelativeRotation.X, InData.RelativeRotation.Y, InData.RelativeRotation.Z));
		SceneComponent->SetRelativeScale3D(InData.RelativeScale);
	}

	if (Component->IsA(UMeshComponent::StaticClass()))
	{
		UMeshComponent* MeshComponent = static_cast<UMeshComponent*>(Component);
		MeshComponent->SetMeshAssetId(InData.MeshAssetId);
		MeshComponent->SetForcedLODLevel(InData.ForcedLODLevel);
		MeshComponent->SetVisibility(InData.bVisible);
		for (const FMaterialOverrideSaveData& Override : InData.MaterialOverrides)
		{
			MeshComponent->SetMaterialOverride(Override.MaterialSlot, Override.MaterialAssetId);
		}
	}

	return Component;
}

void ComponentSerializer::SerializeSceneComponent(const USceneComponent* InComponent, FComponentSaveData* OutData)
{
	if (InComponent == nullptr || OutData == nullptr)
	{
		return;
	}

	OutData->RelativeLocation = InComponent->GetRelativeLocation();
	const FRotator3 Rotation = InComponent->GetRelativeRotation();
	OutData->RelativeRotation = FVector3{Rotation.Pitch, Rotation.Yaw, Rotation.Roll};
	OutData->RelativeScale = InComponent->GetRelativeScale3D();
	const USceneComponent* AttachParent = InComponent->GetAttachParent();
	OutData->ComponentKey = InComponent->GetObjectName();
	OutData->AttachParentKey = AttachParent != nullptr ? AttachParent->GetObjectName() : "";
	OutData->AttachParentName = AttachParent != nullptr ? AttachParent->GetObjectName() : "";
}

void ComponentSerializer::SerializeMeshComponent(const UMeshComponent* InComponent, FComponentSaveData* OutData)
{
	if (InComponent == nullptr || OutData == nullptr)
	{
		return;
	}

	OutData->MeshAssetId = InComponent->GetMeshAssetId();
	for (const auto& Override : InComponent->GetMaterialOverrides())
	{
		FMaterialOverrideSaveData OverrideData;
		OverrideData.MaterialSlot = Override.MaterialSlot;
		OverrideData.MaterialAssetId = Override.MaterialAssetId;
		OutData->MaterialOverrides.push_back(OverrideData);
	}
	OutData->ForcedLODLevel = InComponent->GetForcedLODLevel();
	OutData->bVisible = InComponent->IsVisible();
}
