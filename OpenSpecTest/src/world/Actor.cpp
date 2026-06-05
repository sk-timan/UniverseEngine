#include "world/Actor.h"

#include <utility>

#include "core/ObjectRegistry.h"
#include "components/SceneComponent.h"
#include "math/FTransform.h"

namespace
{
std::unique_ptr<AActor> CreateAActorInstance(uint64_t InObjectId, std::string InObjectName)
{
	return std::make_unique<AActor>(InObjectId, std::move(InObjectName), &AActor::StaticClass());
}
} // namespace

AActor::AActor(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
	: UObject(InObjectId, std::move(InObjectName), InClass != nullptr ? InClass : &AActor::StaticClass())
{
}

const UClass& AActor::StaticClass()
{
	static const UClass Class("AActor", &UObject::StaticClass(), CreateAActorInstance);
	return Class;
}

const FVector3& AActor::GetTransform() const
{
	return ActorTransform_.Position;
}

void AActor::SetTransform(const FVector3& InTransform)
{
	ActorTransform_.Position = InTransform;
	ApplyActorTransformToRoot();
}

const FActorTransform& AActor::GetActorTransform() const
{
	return ActorTransform_;
}

void AActor::SetActorTransform(const FActorTransform& InTransform)
{
	ActorTransform_ = InTransform;
	ApplyActorTransformToRoot();
}

void AActor::ApplyActorTransformToRoot()
{
	if (RootComponent_ == nullptr)
	{
		return;
	}

	RootComponent_->MarkTransformDirty();
}

bool AActor::IsPendingDestroy() const
{
	return bIsPendingDestroy_;
}

void AActor::MarkPendingDestroy()
{
	bIsPendingDestroy_ = true;
}

bool AActor::IsPickable() const
{
	return bIsPickable_;
}

void AActor::SetPickable(bool bInPickable)
{
	bIsPickable_ = bInPickable;
}

void AActor::Tick(float InDeltaSeconds)
{
}

USceneComponent* AActor::GetRootComponent() const
{
	return RootComponent_;
}

void AActor::SetRootComponent(USceneComponent* InRootComponent)
{
	RootComponent_ = InRootComponent;
	if (RootComponent_ != nullptr)
	{
		RootComponent_->SetOwnerActor(const_cast<AActor*>(this));
		ApplyActorTransformToRoot();
	}
}

template<typename T>
T* AActor::FindComponentByClass() const
{
	for (UActorComponent* Component : Components_)
	{
		if (Component->IsA(T::StaticClass()))
		{
			return static_cast<T*>(Component);
		}
	}
	return nullptr;
}

void AActor::AddComponent(UActorComponent* InComponent)
{
	if (InComponent == nullptr)
	{
		return;
	}
	InComponent->SetOwnerActor(this);
	Components_.push_back(InComponent);
	InComponent->OnRegister();
}

bool AActor::RemoveComponent(UActorComponent* InComponent)
{
	if (InComponent == nullptr)
	{
		return false;
	}
	auto It = std::find(Components_.begin(), Components_.end(), InComponent);
	if (It != Components_.end())
	{
		InComponent->OnUnregister();
		InComponent->SetOwnerActor(nullptr);
		Components_.erase(It);
		return true;
	}
	return false;
}

const std::vector<UActorComponent*>& AActor::GetComponents() const
{
	return Components_;
}
