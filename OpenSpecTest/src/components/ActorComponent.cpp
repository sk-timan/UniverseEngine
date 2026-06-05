#include "components/ActorComponent.h"

#include <utility>

#include "components/SceneComponent.h"
#include "core/ObjectRegistry.h"

class AActor;

namespace
{
std::unique_ptr<UActorComponent> CreateActorComponentInstance(uint64_t InObjectId, std::string InObjectName)
{
	return std::make_unique<UActorComponent>(InObjectId, std::move(InObjectName), &UActorComponent::StaticClass());
}
} // namespace

UActorComponent::UActorComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
	: UObject(InObjectId, std::move(InObjectName), InClass != nullptr ? InClass : &UActorComponent::StaticClass())
{
}

const UClass& UActorComponent::StaticClass()
{
	static const UClass Class("UActorComponent", &UObject::StaticClass(), CreateActorComponentInstance);
	return Class;
}

void UActorComponent::SetOwnerActor(AActor* InOwner)
{
	OwnerActor_ = InOwner;
	if (IsA(USceneComponent::StaticClass()))
	{
		static_cast<USceneComponent*>(this)->MarkTransformDirty();
	}
}

AActor* UActorComponent::GetOwnerActor() const
{
	return OwnerActor_;
}

void UActorComponent::OnRegister()
{
}

void UActorComponent::OnUnregister()
{
}

void UActorComponent::Initialize()
{
}

void UActorComponent::Uninitialize()
{
}

void UActorComponent::Tick(float InDeltaSeconds)
{
}

bool UActorComponent::IsRegistered() const
{
	return bIsRegistered_;
}

bool UActorComponent::IsInitialized() const
{
	return bIsInitialized_;
}

bool UActorComponent::IsTickEnabled() const
{
	return bAllowTick_;
}

void UActorComponent::SetTickEnabled(bool bInEnabled)
{
	bAllowTick_ = bInEnabled;
}
