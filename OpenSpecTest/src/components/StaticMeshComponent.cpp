#include "components/StaticMeshComponent.h"

#include <utility>

#include "core/ObjectRegistry.h"
#include "render/asset/StaticMesh.h"

namespace
{
std::unique_ptr<UStaticMeshComponent> CreateStaticMeshComponentInstance(uint64_t InObjectId, std::string InObjectName)
{
	return std::make_unique<UStaticMeshComponent>(InObjectId, std::move(InObjectName), &UStaticMeshComponent::StaticClass());
}
} // namespace

UStaticMeshComponent::UStaticMeshComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
	: UMeshComponent(InObjectId, std::move(InObjectName), InClass != nullptr ? InClass : &UStaticMeshComponent::StaticClass())
{
}

const UClass& UStaticMeshComponent::StaticClass()
{
	static const UClass Class("UStaticMeshComponent", &UMeshComponent::StaticClass(), CreateStaticMeshComponentInstance);
	return Class;
}

void UStaticMeshComponent::SetStaticMesh(UStaticMesh* InStaticMesh)
{
	StaticMesh_ = InStaticMesh;
	if (StaticMesh_ != nullptr)
	{
		SetMeshAsset(StaticMesh_);
	}
}

UStaticMesh* UStaticMeshComponent::GetStaticMesh() const
{
	return StaticMesh_;
}

void UStaticMeshComponent::ClearLoadedMesh()
{
	StaticMesh_ = nullptr;
	UMeshComponent::ClearLoadedMesh();
}

void UStaticMeshComponent::CreateRenderState(FPrimitiveRenderState* OutRenderState)
{
	UMeshComponent::CreateRenderState(OutRenderState);

	if (OutRenderState == nullptr || StaticMesh_ == nullptr)
	{
		return;
	}

	OutRenderState->bIsValid = true;
}

void UStaticMeshComponent::UpdateRenderState(FPrimitiveRenderState* InOutRenderState)
{
	UMeshComponent::UpdateRenderState(InOutRenderState);

	if (InOutRenderState == nullptr)
	{
		return;
	}
}

void UStaticMeshComponent::DestroyRenderState(FPrimitiveRenderState* InRenderState)
{
	UMeshComponent::DestroyRenderState(InRenderState);

	if (InRenderState == nullptr)
	{
		return;
	}

	InRenderState->bIsValid = false;
}
