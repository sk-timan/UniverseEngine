#include "components/SkeletalMeshComponent.h"

#include <utility>

#include "core/ObjectRegistry.h"
#include "render/asset/SkeletalMesh.h"

namespace
{
std::unique_ptr<USkeletalMeshComponent> CreateSkeletalMeshComponentInstance(uint64_t InObjectId, std::string InObjectName)
{
	return std::make_unique<USkeletalMeshComponent>(InObjectId, std::move(InObjectName), &USkeletalMeshComponent::StaticClass());
}
} // namespace

USkeletalMeshComponent::USkeletalMeshComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
	: USkinnedMeshComponent(InObjectId, std::move(InObjectName), InClass != nullptr ? InClass : &USkeletalMeshComponent::StaticClass())
{
}

const UClass& USkeletalMeshComponent::StaticClass()
{
	static const UClass Class("USkeletalMeshComponent", &USkinnedMeshComponent::StaticClass(), CreateSkeletalMeshComponentInstance);
	return Class;
}

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
	SkeletalMesh_ = InSkeletalMesh;
	if (SkeletalMesh_ != nullptr)
	{
		SetMeshAsset(SkeletalMesh_);
	}
}

USkeletalMesh* USkeletalMeshComponent::GetSkeletalMesh() const
{
	return SkeletalMesh_;
}

void USkeletalMeshComponent::ClearLoadedMesh()
{
	SkeletalMesh_ = nullptr;
	UMeshComponent::ClearLoadedMesh();
}

void USkeletalMeshComponent::CreateRenderState(FPrimitiveRenderState* OutRenderState)
{
	USkinnedMeshComponent::CreateRenderState(OutRenderState);

	if (OutRenderState == nullptr || SkeletalMesh_ == nullptr)
	{
		return;
	}

	OutRenderState->bIsValid = true;
}

void USkeletalMeshComponent::UpdateRenderState(FPrimitiveRenderState* InOutRenderState)
{
	USkinnedMeshComponent::UpdateRenderState(InOutRenderState);

	if (InOutRenderState == nullptr)
	{
		return;
	}
}
