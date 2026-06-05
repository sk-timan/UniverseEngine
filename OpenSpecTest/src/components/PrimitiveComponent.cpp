#include "components/PrimitiveComponent.h"

#include <algorithm>
#include <utility>

#include "core/ObjectRegistry.h"

namespace
{
std::unique_ptr<UPrimitiveComponent> CreatePrimitiveComponentInstance(uint64_t InObjectId, std::string InObjectName)
{
	return std::make_unique<UPrimitiveComponent>(InObjectId, std::move(InObjectName), &UPrimitiveComponent::StaticClass());
}
} // namespace

UPrimitiveComponent::UPrimitiveComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
	: USceneComponent(InObjectId, std::move(InObjectName), InClass != nullptr ? InClass : &UPrimitiveComponent::StaticClass())
{
}

const UClass& UPrimitiveComponent::StaticClass()
{
	static const UClass Class("UPrimitiveComponent", &USceneComponent::StaticClass(), CreatePrimitiveComponentInstance);
	return Class;
}

void UPrimitiveComponent::SetVisibility(bool bInVisible)
{
	bIsVisible_ = bInVisible;
}

bool UPrimitiveComponent::IsVisible() const
{
	return bIsVisible_;
}

void UPrimitiveComponent::SetCullDistance(float InCullDistance)
{
	CullDistance_ = InCullDistance;
}

float UPrimitiveComponent::GetCullDistance() const
{
	return CullDistance_;
}

UPrimitiveComponent::FBounds UPrimitiveComponent::GetBounds() const
{
	return CachedBounds_;
}

void UPrimitiveComponent::CreateRenderState(FPrimitiveRenderState* OutRenderState)
{
	if (OutRenderState == nullptr)
	{
		return;
	}
	OutRenderState->bIsValid = true;
}

void UPrimitiveComponent::UpdateRenderState(FPrimitiveRenderState* InOutRenderState)
{
}

void UPrimitiveComponent::DestroyRenderState(FPrimitiveRenderState* InRenderState)
{
	if (InRenderState == nullptr)
	{
		return;
	}
	InRenderState->bIsValid = false;
}

UPrimitiveComponent::FPrimitiveRenderState* UPrimitiveComponent::GetRenderState()
{
	return RenderState_;
}

const UPrimitiveComponent::FPrimitiveRenderState* UPrimitiveComponent::GetRenderState() const
{
	return RenderState_;
}
