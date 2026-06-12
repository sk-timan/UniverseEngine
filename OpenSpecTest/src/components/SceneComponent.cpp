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

void USceneComponent::RebuildRelativeTransformCache() const
{
	CachedRelativeTransform_ = FTransform(RelativeLocation_, RelativeRotation_, RelativeScale_);
	bRelativeTransformCacheDirty_ = false;
}

void USceneComponent::SetRelativeTransform(const FTransform& InTransform)
{
	RelativeLocation_ = InTransform.GetLocation();
	RelativeRotation_ = InTransform.GetRotation();
	RelativeScale_ = InTransform.GetScale();
	bRelativeTransformCacheDirty_ = true;
	MarkTransformDirty();
	OnUpdateTransform();
}

const FTransform& USceneComponent::GetRelativeTransform() const
{
	if (bRelativeTransformCacheDirty_)
	{
		RebuildRelativeTransformCache();
	}
	return CachedRelativeTransform_;
}

void USceneComponent::SetRelativeLocation(const FVector3& InLocation)
{
	RelativeLocation_ = InLocation;
	bRelativeTransformCacheDirty_ = true;
	MarkTransformDirty();
	OnUpdateTransform();
}

const FVector3& USceneComponent::GetRelativeLocation() const
{
	return RelativeLocation_;
}

void USceneComponent::SetRelativeRotation(const FRotator3& InRotation)
{
	RelativeRotation_ = InRotation;
	bRelativeTransformCacheDirty_ = true;
	MarkTransformDirty();
	OnUpdateTransform();
}

FRotator3 USceneComponent::GetRelativeRotation() const
{
	return RelativeRotation_;
}

void USceneComponent::SetRelativeScale3D(const FVector3& InScale)
{
	RelativeScale_ = InScale;
	bRelativeTransformCacheDirty_ = true;
	MarkTransformDirty();
	OnUpdateTransform();
}

const FVector3& USceneComponent::GetRelativeScale3D() const
{
	return RelativeScale_;
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

void USceneComponent::NotifyRelativeTransformEdited()
{
	bRelativeTransformCacheDirty_ = true;
	MarkTransformDirty();
	OnUpdateTransform();
}

void USceneComponent::UpdateWorldTransform() const
{
	const FTransform& RelativeTransform = GetRelativeTransform();
	if (IsIdentityRelativeTransform(RelativeTransform))
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
			CachedWorldTransform_ = RelativeTransform;
		}
	}
	else if (AttachParent_ != nullptr)
	{
		CachedWorldTransform_ = FTransform::Combine(AttachParent_->GetWorldTransform(), RelativeTransform);
	}
	else if (AActor* Owner = GetOwnerActor())
	{
		CachedWorldTransform_ =
			FTransform::Combine(Owner->GetActorTransform().ToSceneTransform(), RelativeTransform);
	}
	else
	{
		CachedWorldTransform_ = RelativeTransform;
	}
	bWorldTransformDirty_ = false;
}
