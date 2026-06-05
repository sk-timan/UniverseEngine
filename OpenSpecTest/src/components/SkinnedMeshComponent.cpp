#include "components/SkinnedMeshComponent.h"

#include <utility>

#include "core/ObjectRegistry.h"

namespace
{
std::unique_ptr<USkinnedMeshComponent> CreateSkinnedMeshComponentInstance(uint64_t InObjectId, std::string InObjectName)
{
	return std::make_unique<USkinnedMeshComponent>(InObjectId, std::move(InObjectName), &USkinnedMeshComponent::StaticClass());
}
} // namespace

USkinnedMeshComponent::USkinnedMeshComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
	: UMeshComponent(InObjectId, std::move(InObjectName), InClass != nullptr ? InClass : &USkinnedMeshComponent::StaticClass())
{
}

const UClass& USkinnedMeshComponent::StaticClass()
{
	static const UClass Class("USkinnedMeshComponent", &UMeshComponent::StaticClass(), CreateSkinnedMeshComponentInstance);
	return Class;
}

void USkinnedMeshComponent::SetBoneTransforms(const std::vector<FBoneTransform>& InBoneTransforms)
{
	BoneTransforms_ = InBoneTransforms;
}

const std::vector<USkinnedMeshComponent::FBoneTransform>& USkinnedMeshComponent::GetBoneTransforms() const
{
	return BoneTransforms_;
}
