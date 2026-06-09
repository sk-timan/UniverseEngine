#include "components/MeshComponent.h"

#include <algorithm>
#include <utility>

#include "asset/AssetRegistry.h"
#include "asset/SoftObjectPath.h"
#include "core/ObjectRegistry.h"
#include "render/asset/StreamableRenderAsset.h"

namespace
{
std::unique_ptr<UMeshComponent> CreateMeshComponentInstance(uint64_t InObjectId, std::string InObjectName)
{
	return std::make_unique<UMeshComponent>(InObjectId, std::move(InObjectName), &UMeshComponent::StaticClass());
}
} // namespace

UMeshComponent::UMeshComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
	: UPrimitiveComponent(InObjectId, std::move(InObjectName), InClass != nullptr ? InClass : &UMeshComponent::StaticClass())
{
}

const UClass& UMeshComponent::StaticClass()
{
	static const UClass Class("UMeshComponent", &UPrimitiveComponent::StaticClass(), CreateMeshComponentInstance);
	return Class;
}

void UMeshComponent::SetMeshAssetId(const std::string& InAssetId)
{
	MeshAssetId_ = InAssetId;
	if (const std::optional<FAssetRegistryEntry> RegistryEntry = FAssetRegistry::Get().FindBySoftPath(InAssetId))
	{
		MeshAssetGuid_ = RegistryEntry->Guid;
	}
}

const std::string& UMeshComponent::GetMeshAssetId() const
{
	return MeshAssetId_;
}

void UMeshComponent::SetMeshAssetGuid(const std::string& InGuid)
{
	MeshAssetGuid_ = InGuid;
}

const std::string& UMeshComponent::GetMeshAssetGuid() const
{
	return MeshAssetGuid_;
}

void UMeshComponent::SetMeshAsset(UStreamableRenderAsset* InAsset)
{
	MeshAsset_ = InAsset;
	if (InAsset != nullptr)
	{
		const std::string SoftPath =
			FSoftObjectPath::Build(InAsset->GetAssetPath().string(), InAsset->GetObjectName());
		SetMeshAssetId(SoftPath);
	}
}

UStreamableRenderAsset* UMeshComponent::GetMeshAsset() const
{
	return MeshAsset_;
}

void UMeshComponent::ClearLoadedMesh()
{
	MeshAsset_ = nullptr;
}

void UMeshComponent::SetMaterialOverride(int32_t InSlot, const std::string& InMaterialAssetId)
{
	for (auto& Override : MaterialOverrides_)
	{
		if (Override.MaterialSlot == InSlot)
		{
			Override.MaterialAssetId = InMaterialAssetId;
			return;
		}
	}

	FMaterialOverride NewOverride;
	NewOverride.MaterialSlot = InSlot;
	NewOverride.MaterialAssetId = InMaterialAssetId;
	MaterialOverrides_.push_back(NewOverride);
}

void UMeshComponent::ClearMaterialOverride(int32_t InSlot)
{
	MaterialOverrides_.erase(
		std::remove_if(MaterialOverrides_.begin(), MaterialOverrides_.end(),
			[InSlot](const FMaterialOverride& InOverride)
			{
				return InOverride.MaterialSlot == InSlot;
			}),
		MaterialOverrides_.end());
}

const std::vector<UMeshComponent::FMaterialOverride>& UMeshComponent::GetMaterialOverrides() const
{
	return MaterialOverrides_;
}

void UMeshComponent::SetForcedLODLevel(int32_t InLODLevel)
{
	ForcedLODLevel_ = InLODLevel;
}

int32_t UMeshComponent::GetForcedLODLevel() const
{
	return ForcedLODLevel_;
}

int32_t UMeshComponent::GetCurrentLODLevel() const
{
	if (ForcedLODLevel_ > 0)
	{
		return ForcedLODLevel_;
	}
	return 0;
}

UMeshComponent::FBounds UMeshComponent::GetBounds() const
{
	if (MeshAsset_ != nullptr)
	{
		UStreamableRenderAsset::FBounds AssetBounds = MeshAsset_->GetBounds();
		return {AssetBounds.Origin, AssetBounds.Extent, AssetBounds.SphereRadius};
	}
	return UPrimitiveComponent::GetBounds();
}
