#include "render/asset/SkinnedAsset.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/ObjectRegistry.h"

namespace
{
std::unique_ptr<USkinnedAsset> CreateSkinnedAssetInstance(uint64_t InObjectId, std::string InObjectName)
{
	return std::make_unique<USkinnedAsset>(InObjectId, std::move(InObjectName), &USkinnedAsset::StaticClass());
}
} // namespace

USkinnedAsset::USkinnedAsset(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
	: UStreamableRenderAsset(InObjectId, std::move(InObjectName), InClass != nullptr ? InClass : &USkinnedAsset::StaticClass())
{
}

const UClass& USkinnedAsset::StaticClass()
{
	static const UClass Class("USkinnedAsset", &UStreamableRenderAsset::StaticClass(), CreateSkinnedAssetInstance);
	return Class;
}

void USkinnedAsset::SetSkeleton(const std::vector<FBone>& InBones)
{
	Skeleton_ = InBones;
}

const std::vector<USkinnedAsset::FBone>& USkinnedAsset::GetSkeleton() const
{
	return Skeleton_;
}

int32_t USkinnedAsset::FindBoneIndex(const std::string& InBoneName) const
{
	for (size_t i = 0; i < Skeleton_.size(); ++i)
	{
		if (Skeleton_[i].Name == InBoneName)
		{
			return static_cast<int32_t>(i);
		}
	}
	return -1;
}

void USkinnedAsset::SetSkinVertices(const std::vector<FSkinVertex>& InVertices)
{
	SetSkinVertices(InVertices, true);
}

void USkinnedAsset::SetSkinVertices(const std::vector<FSkinVertex>& InVertices, bool bUpdateBounds)
{
	SkinVertices_ = InVertices;
	if (bUpdateBounds)
	{
		RebuildBoundsFromSkinVertices();
	}
}

void USkinnedAsset::RebuildBoundsFromSkinVertices()
{
	TotalBounds_.Origin = {0.0f, 0.0f, 0.0f};
	TotalBounds_.Extent = {0.0f, 0.0f, 0.0f};
	TotalBounds_.SphereRadius = 0.0f;

	if (SkinVertices_.empty())
	{
		return;
	}

	float MinX = SkinVertices_[0].Position.X;
	float MaxX = SkinVertices_[0].Position.X;
	float MinY = SkinVertices_[0].Position.Y;
	float MaxY = SkinVertices_[0].Position.Y;
	float MinZ = SkinVertices_[0].Position.Z;
	float MaxZ = SkinVertices_[0].Position.Z;

	for (const FSkinVertex& Vertex : SkinVertices_)
	{
		MinX = std::min(MinX, Vertex.Position.X);
		MaxX = std::max(MaxX, Vertex.Position.X);
		MinY = std::min(MinY, Vertex.Position.Y);
		MaxY = std::max(MaxY, Vertex.Position.Y);
		MinZ = std::min(MinZ, Vertex.Position.Z);
		MaxZ = std::max(MaxZ, Vertex.Position.Z);
	}

	TotalBounds_.Origin.X = (MinX + MaxX) * 0.5f;
	TotalBounds_.Origin.Y = (MinY + MaxY) * 0.5f;
	TotalBounds_.Origin.Z = (MinZ + MaxZ) * 0.5f;
	TotalBounds_.Extent.X = (MaxX - MinX) * 0.5f;
	TotalBounds_.Extent.Y = (MaxY - MinY) * 0.5f;
	TotalBounds_.Extent.Z = (MaxZ - MinZ) * 0.5f;

	const float HalfWidth = TotalBounds_.Extent.X;
	const float HalfHeight = TotalBounds_.Extent.Y;
	const float HalfDepth = TotalBounds_.Extent.Z;
	TotalBounds_.SphereRadius =
		std::sqrt(HalfWidth * HalfWidth + HalfHeight * HalfHeight + HalfDepth * HalfDepth);
}

UStreamableRenderAsset::FBounds USkinnedAsset::GetBounds() const
{
	return TotalBounds_;
}

const std::vector<USkinnedAsset::FSkinVertex>& USkinnedAsset::GetSkinVertices() const
{
	return SkinVertices_;
}

bool USkinnedAsset::HasResidentGeometryData() const
{
	return !SkinVertices_.empty();
}
