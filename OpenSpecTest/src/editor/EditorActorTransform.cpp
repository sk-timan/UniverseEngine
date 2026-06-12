#include "editor/EditorActorTransform.h"

#include "components/SceneComponent.h"
#include "world/Actor.h"

AActor* FEditorActorTransform::GetAttachParentActor(const AActor* InActor)
{
	if (InActor == nullptr)
	{
		return nullptr;
	}

	USceneComponent* RootComponent = InActor->GetRootComponent();
	if (RootComponent == nullptr)
	{
		return nullptr;
	}

	USceneComponent* AttachParent = RootComponent->GetAttachParent();
	if (AttachParent == nullptr)
	{
		return nullptr;
	}

	AActor* ParentActor = AttachParent->GetOwnerActor();
	if (ParentActor == nullptr || ParentActor == InActor)
	{
		return nullptr;
	}

	return ParentActor;
}

bool FEditorActorTransform::IsAttachedToActor(const AActor* InActor)
{
	return GetAttachParentActor(InActor) != nullptr;
}

FTransform FEditorActorTransform::GetActorWorldTransform(const AActor* InActor)
{
	if (InActor == nullptr)
	{
		return FTransform();
	}

	USceneComponent* RootComponent = InActor->GetRootComponent();
	if (RootComponent != nullptr)
	{
		return RootComponent->GetWorldTransform();
	}

	return InActor->GetActorTransform().ToSceneTransform();
}

FActorTransform FEditorActorTransform::GetEditableTransform(const AActor* InActor)
{
	if (InActor == nullptr)
	{
		return FActorTransform::Identity();
	}

	if (IsAttachedToActor(InActor))
	{
		USceneComponent* RootComponent = InActor->GetRootComponent();
		if (RootComponent != nullptr)
		{
			return FActorTransform::FromSceneTransform(RootComponent->GetRelativeTransform());
		}
	}

	USceneComponent* RootComponent = InActor->GetRootComponent();
	if (RootComponent != nullptr)
	{
		return FActorTransform::FromSceneTransform(RootComponent->GetRelativeTransform());
	}

	return InActor->GetActorTransform();
}

bool FEditorActorTransform::SetEditableTransform(AActor* InActor, const FActorTransform& InTransform)
{
	if (InActor == nullptr)
	{
		return false;
	}

	FActorTransform NormalizedTransform = InTransform;
	NormalizedTransform.Rotation = NormalizedTransform.Rotation.GetNormalized();

	if (IsAttachedToActor(InActor))
	{
		USceneComponent* RootComponent = InActor->GetRootComponent();
		if (RootComponent == nullptr)
		{
			return false;
		}

		RootComponent->SetRelativeTransform(NormalizedTransform.ToSceneTransform());
		return true;
	}

	InActor->SetActorTransform(NormalizedTransform);
	return true;
}

bool FEditorActorTransform::SetActorWorldTransform(AActor* InActor, const FTransform& InWorldTransform)
{
	if (InActor == nullptr)
	{
		return false;
	}

	USceneComponent* RootComponent = InActor->GetRootComponent();
	if (RootComponent == nullptr)
	{
		return false;
	}

	if (IsAttachedToActor(InActor))
	{
		AActor* ParentActor = GetAttachParentActor(InActor);
		if (ParentActor == nullptr)
		{
			return false;
		}

		const FTransform ParentWorldTransform = GetActorWorldTransform(ParentActor);
		const FTransform RelativeTransform =
			FTransform::ComputeRelative(ParentWorldTransform, InWorldTransform);
		RootComponent->SetRelativeTransform(RelativeTransform);
		return true;
	}

	InActor->SetActorTransform(FActorTransform::FromSceneTransform(InWorldTransform));
	RootComponent->SetRelativeTransform(FTransform());
	return true;
}

bool FEditorActorTransform::WouldCreateAttachmentCycle(
	const AActor* InChildActor,
	const AActor* InNewParentActor)
{
	if (InChildActor == nullptr || InNewParentActor == nullptr || InChildActor == InNewParentActor)
	{
		return InChildActor == InNewParentActor;
	}

	const AActor* Current = InNewParentActor;
	while (Current != nullptr)
	{
		if (Current == InChildActor)
		{
			return true;
		}
		Current = GetAttachParentActor(Current);
	}

	return false;
}

bool FEditorActorTransform::ReparentActor(
	AActor* InChildActor,
	AActor* InNewParentActor,
	std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}

	if (InChildActor == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Child actor is null.";
		}
		return false;
	}

	if (InNewParentActor == InChildActor)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Cannot attach actor to itself.";
		}
		return false;
	}

	if (InNewParentActor != nullptr && WouldCreateAttachmentCycle(InChildActor, InNewParentActor))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Attachment would create a cycle.";
		}
		return false;
	}

	USceneComponent* ChildRoot = InChildActor->GetRootComponent();
	if (ChildRoot == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Child actor has no root component.";
		}
		return false;
	}

	const FTransform ChildWorldTransform = GetActorWorldTransform(InChildActor);
	if (ChildRoot->GetAttachParent() != nullptr)
	{
		ChildRoot->DetachFromComponent();
	}

	if (InNewParentActor == nullptr)
	{
		InChildActor->SetActorTransform(FActorTransform::FromSceneTransform(ChildWorldTransform));
		ChildRoot->SetRelativeTransform(FTransform());
		return true;
	}

	USceneComponent* ParentRoot = InNewParentActor->GetRootComponent();
	if (ParentRoot == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Parent actor has no root component.";
		}
		return false;
	}

	ChildRoot->AttachToComponent(ParentRoot);
	const FTransform ParentWorldTransform = ParentRoot->GetWorldTransform();
	const FTransform RelativeTransform =
		FTransform::ComputeRelative(ParentWorldTransform, ChildWorldTransform);
	InChildActor->SetActorTransform(FActorTransform::Identity());
	ChildRoot->SetRelativeTransform(RelativeTransform);
	return true;
}
