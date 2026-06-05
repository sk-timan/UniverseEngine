#include "components/SceneComponent.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/ObjectRegistry.h"
#include "world/Actor.h"

namespace
{
constexpr float kTransformEpsilon = 1.0e-4f;

std::unique_ptr<USceneComponent> CreateSceneComponentInstance(uint64_t InObjectId, std::string InObjectName)
{
	return std::make_unique<USceneComponent>(InObjectId, std::move(InObjectName), &USceneComponent::StaticClass());
}

bool IsNearlyZero(float InValue)
{
	return std::fabs(InValue) <= kTransformEpsilon;
}

bool IsNearlyOne(float InValue)
{
	return std::fabs(InValue - 1.0f) <= kTransformEpsilon;
}

bool IsIdentityRelativeTransform(const FTransform& InTransform)
{
	const FVector3 Location = InTransform.GetLocation();
	const FRotator3 Rotation = InTransform.GetRotation();
	const FVector3 Scale = InTransform.GetScale();
	return IsNearlyZero(Location.X) && IsNearlyZero(Location.Y) && IsNearlyZero(Location.Z) &&
		IsNearlyZero(Rotation.Pitch) && IsNearlyZero(Rotation.Yaw) && IsNearlyZero(Rotation.Roll) &&
		IsNearlyOne(Scale.X) && IsNearlyOne(Scale.Y) && IsNearlyOne(Scale.Z);
}

} // namespace

USceneComponent::USceneComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
	: UActorComponent(InObjectId, std::move(InObjectName), InClass != nullptr ? InClass : &USceneComponent::StaticClass())
{
}

const UClass& USceneComponent::StaticClass()
{
	static const UClass Class("USceneComponent", &UActorComponent::StaticClass(), CreateSceneComponentInstance);
	return Class;
}

void USceneComponent::SetRelativeTransform(const FTransform& InTransform)
{
	RelativeTransform_ = InTransform;
	MarkTransformDirty();
	OnUpdateTransform();
}

const FTransform& USceneComponent::GetRelativeTransform() const
{
	return RelativeTransform_;
}

void USceneComponent::SetRelativeLocation(const FVector3& InLocation)
{
	RelativeTransform_.SetLocation(InLocation);
	MarkTransformDirty();
	OnUpdateTransform();
}

const FVector3& USceneComponent::GetRelativeLocation() const
{
	return RelativeTransform_.GetLocation();
}

void USceneComponent::SetRelativeRotation(const FRotator3& InRotation)
{
	RelativeTransform_.SetRotation(InRotation);
	MarkTransformDirty();
	OnUpdateTransform();
}

FRotator3 USceneComponent::GetRelativeRotation() const
{
	return RelativeTransform_.GetRotation();
}

void USceneComponent::SetRelativeScale3D(const FVector3& InScale)
{
	RelativeTransform_.SetScale(InScale);
	MarkTransformDirty();
	OnUpdateTransform();
}

const FVector3& USceneComponent::GetRelativeScale3D() const
{
	return RelativeTransform_.GetScale();
}

FTransform USceneComponent::GetWorldTransform() const
{
	if (bWorldTransformDirty_)
	{
		UpdateWorldTransform();
	}
	return CachedWorldTransform_;
}

FVector3 USceneComponent::GetWorldLocation() const
{
	return GetWorldTransform().GetLocation();
}

void USceneComponent::AttachToComponent(USceneComponent* InParent)
{
	if (InParent == this)
	{
		return;
	}

	if (AttachParent_ != nullptr)
	{
		DetachFromComponent();
	}

	AttachParent_ = InParent;
	if (AttachParent_ != nullptr)
	{
		AttachParent_->ChildComponents_.push_back(this);
	}

	MarkTransformDirty();
}

void USceneComponent::DetachFromComponent()
{
	if (AttachParent_ != nullptr)
	{
		auto It = std::find(AttachParent_->ChildComponents_.begin(), AttachParent_->ChildComponents_.end(), this);
		if (It != AttachParent_->ChildComponents_.end())
		{
			AttachParent_->ChildComponents_.erase(It);
		}
		AttachParent_ = nullptr;
	}

	MarkTransformDirty();
}

USceneComponent* USceneComponent::GetAttachParent() const
{
	return AttachParent_;
}

void USceneComponent::GetChildComponents(std::vector<USceneComponent*>& OutChildren) const
{
	OutChildren = ChildComponents_;
}

void USceneComponent::OnUpdateTransform()
{
}

void USceneComponent::MarkTransformDirty()
{
	bWorldTransformDirty_ = true;
	for (USceneComponent* Child : ChildComponents_)
	{
		if (Child != nullptr)
		{
			Child->MarkTransformDirty();
		}
	}
}

void USceneComponent::UpdateWorldTransform() const
{
	if (IsIdentityRelativeTransform(RelativeTransform_))
	{
		if (AttachParent_ != nullptr)
		{
			CachedWorldTransform_ = AttachParent_->GetWorldTransform();
		}
		else if (AActor* Owner = GetOwnerActor())
		{
			CachedWorldTransform_ = Owner->GetActorTransform().ToSceneTransform();
		}
		else
		{
			CachedWorldTransform_ = RelativeTransform_;
		}
	}
	else if (AttachParent_ != nullptr)
	{
		CachedWorldTransform_ =
			FTransform::Combine(AttachParent_->GetWorldTransform(), RelativeTransform_);
	}
	else if (AActor* Owner = GetOwnerActor())
	{
		CachedWorldTransform_ =
			FTransform::Combine(Owner->GetActorTransform().ToSceneTransform(), RelativeTransform_);
	}
	else
	{
		CachedWorldTransform_ = RelativeTransform_;
	}
	bWorldTransformDirty_ = false;
}
