#include "editor/EditorPicking.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

#include <DirectXMath.h>

#include "components/MeshComponent.h"
#include "components/PrimitiveComponent.h"
#include "components/SkeletalMeshComponent.h"
#include "components/StaticMeshComponent.h"
#include "editor/EditorPickBvh.h"
#include "editor/EditorRayIntersection.h"
#include "editor/EditorTriangleBvh.h"
#include "render/asset/SkeletalMesh.h"
#include "render/asset/StaticMesh.h"
#include "world/Actor.h"
#include "world/Level.h"

namespace
{
constexpr float kDefaultBoundsRadius = 2.0f;
constexpr float kMinPickHalfExtentWorld = 1.5f;

FEditorTriangleBvhCache GTriangleBvhCache;

bool HasValidBounds(const UPrimitiveComponent::FBounds& InBounds)
{
	return InBounds.Extent.X > 0.0f || InBounds.Extent.Y > 0.0f || InBounds.Extent.Z > 0.0f;
}

UPrimitiveComponent::FBounds GetPrimitiveLocalBounds(const UPrimitiveComponent* InPrimitive)
{
	UPrimitiveComponent::FBounds LocalBounds = InPrimitive->GetBounds();
	if (const UMeshComponent* MeshComponent = dynamic_cast<const UMeshComponent*>(InPrimitive))
	{
		LocalBounds = MeshComponent->GetBounds();
	}
	return LocalBounds;
}

UPrimitiveComponent::FBounds GetPrimitivePickLocalBounds(const UPrimitiveComponent* InPrimitive)
{
	return GetPrimitiveLocalBounds(InPrimitive);
}

void ApplyPickObbMinimumSize(FEditorOrientedBox* InOutObb, const FVector3& InCameraPosition)
{
	if (InOutObb == nullptr)
	{
		return;
	}

	const float Distance = std::max((InOutObb->Center - InCameraPosition).Length(), 0.5f);
	const float MinHalfExtent = std::max(kMinPickHalfExtentWorld, Distance * 0.008f);
	InOutObb->HalfExtents.X = std::max(InOutObb->HalfExtents.X, MinHalfExtent);
	InOutObb->HalfExtents.Y = std::max(InOutObb->HalfExtents.Y, MinHalfExtent);
	InOutObb->HalfExtents.Z = std::max(InOutObb->HalfExtents.Z, MinHalfExtent);
}

UPrimitiveComponent::FBounds ToPrimitiveBounds(const UStreamableRenderAsset::FBounds& InBounds)
{
	return {InBounds.Origin, InBounds.Extent, InBounds.SphereRadius};
}

FEditorOrientedBox BuildPrimitiveWorldObb(
	const UPrimitiveComponent* InPrimitive,
	const UPrimitiveComponent::FBounds& InLocalBounds)
{
	FEditorOrientedBox Result{};
	const DirectX::XMMATRIX WorldMatrix = InPrimitive->GetWorldTransform().ToMatrix();
	const DirectX::XMVECTOR LocalCenter = DirectX::XMVectorSet(
		InLocalBounds.Origin.X,
		InLocalBounds.Origin.Y,
		InLocalBounds.Origin.Z,
		1.0f);
	const DirectX::XMVECTOR WorldCenter = DirectX::XMVector3TransformCoord(LocalCenter, WorldMatrix);
	Result.Center = FVector3(
		DirectX::XMVectorGetX(WorldCenter),
		DirectX::XMVectorGetY(WorldCenter),
		DirectX::XMVectorGetZ(WorldCenter));

	const DirectX::XMVECTOR AxisX = WorldMatrix.r[0];
	const DirectX::XMVECTOR AxisY = WorldMatrix.r[1];
	const DirectX::XMVECTOR AxisZ = WorldMatrix.r[2];
	const float AxisScaleX = DirectX::XMVectorGetX(DirectX::XMVector3Length(AxisX));
	const float AxisScaleY = DirectX::XMVectorGetX(DirectX::XMVector3Length(AxisY));
	const float AxisScaleZ = DirectX::XMVectorGetX(DirectX::XMVector3Length(AxisZ));

	Result.AxisX = FVector3(
		DirectX::XMVectorGetX(AxisX),
		DirectX::XMVectorGetY(AxisX),
		DirectX::XMVectorGetZ(AxisX)).Normalized();
	Result.AxisY = FVector3(
		DirectX::XMVectorGetX(AxisY),
		DirectX::XMVectorGetY(AxisY),
		DirectX::XMVectorGetZ(AxisY)).Normalized();
	Result.AxisZ = FVector3(
		DirectX::XMVectorGetX(AxisZ),
		DirectX::XMVectorGetY(AxisZ),
		DirectX::XMVectorGetZ(AxisZ)).Normalized();
	Result.HalfExtents = FVector3(
		InLocalBounds.Extent.X * AxisScaleX,
		InLocalBounds.Extent.Y * AxisScaleY,
		InLocalBounds.Extent.Z * AxisScaleZ);
	return Result;
}

bool IsPrimitiveBeyondCullDistance(
	const UPrimitiveComponent* InPrimitive,
	const FEditorOrientedBox& InWorldObb,
	const FVector3& InCameraPosition)
{
	const float CullDistance = InPrimitive->GetCullDistance();
	if (CullDistance <= 0.0f)
	{
		return false;
	}

	const FVector3 Delta = InWorldObb.Center - InCameraPosition;
	return Delta.Length() > CullDistance;
}

bool TryUpdateBestPickHit(
	float InHitT,
	uint64_t InActorObjectId,
	float* InOutBestT,
	uint64_t* InOutBestActorId)
{
	if (InOutBestT == nullptr || InOutBestActorId == nullptr)
	{
		return false;
	}

	if (InHitT < *InOutBestT)
	{
		*InOutBestT = InHitT;
		*InOutBestActorId = InActorObjectId;
		return true;
	}
	return false;
}

float ComputeWorldRayDistanceFromLocalHit(
	const FWorldRay& InWorldRay,
	const DirectX::XMMATRIX& InWorldMatrix,
	const FWorldRay& InLocalRay,
	float InLocalHitT)
{
	using namespace DirectX;
	const XMVECTOR LocalOrigin = XMVectorSet(
		InLocalRay.Origin.X,
		InLocalRay.Origin.Y,
		InLocalRay.Origin.Z,
		1.0f);
	const XMVECTOR LocalDirection = XMVectorSet(
		InLocalRay.Direction.X,
		InLocalRay.Direction.Y,
		InLocalRay.Direction.Z,
		0.0f);
	const XMVECTOR LocalHitPoint = XMVectorAdd(LocalOrigin, XMVectorScale(LocalDirection, InLocalHitT));
	const XMVECTOR WorldHitPoint = XMVector3TransformCoord(LocalHitPoint, InWorldMatrix);
	const XMVECTOR WorldOrigin = XMVectorSet(
		InWorldRay.Origin.X,
		InWorldRay.Origin.Y,
		InWorldRay.Origin.Z,
		1.0f);
	const XMVECTOR WorldDirection = XMVectorSet(
		InWorldRay.Direction.X,
		InWorldRay.Direction.Y,
		InWorldRay.Direction.Z,
		0.0f);
	const XMVECTOR ToHit = XMVectorSubtract(WorldHitPoint, WorldOrigin);
	return XMVectorGetX(XMVector3Dot(ToHit, WorldDirection));
}

float ComputeLocalPickMaxTFromWorldBestT(
	const FWorldRay& InWorldRay,
	const DirectX::XMMATRIX& InWorldMatrix,
	const FWorldRay& InLocalRay,
	float InWorldBestT)
{
	if (!std::isfinite(InWorldBestT) || InWorldBestT >= std::numeric_limits<float>::max() * 0.5f)
	{
		return std::numeric_limits<float>::max();
	}

	using namespace DirectX;
	const XMVECTOR WorldOrigin = XMVectorSet(
		InWorldRay.Origin.X,
		InWorldRay.Origin.Y,
		InWorldRay.Origin.Z,
		1.0f);
	const XMVECTOR WorldDirection = XMVectorSet(
		InWorldRay.Direction.X,
		InWorldRay.Direction.Y,
		InWorldRay.Direction.Z,
		0.0f);
	const XMVECTOR WorldHitPoint = XMVectorAdd(WorldOrigin, XMVectorScale(WorldDirection, InWorldBestT));

	const XMVECTOR LocalOrigin = XMVectorSet(
		InLocalRay.Origin.X,
		InLocalRay.Origin.Y,
		InLocalRay.Origin.Z,
		1.0f);
	const XMVECTOR LocalDirection = XMVectorSet(
		InLocalRay.Direction.X,
		InLocalRay.Direction.Y,
		InLocalRay.Direction.Z,
		0.0f);
	const XMMATRIX InvWorldMatrix = XMMatrixInverse(nullptr, InWorldMatrix);
	const XMVECTOR LocalHitPoint = XMVector3TransformCoord(WorldHitPoint, InvWorldMatrix);
	const XMVECTOR ToHit = XMVectorSubtract(LocalHitPoint, LocalOrigin);
	const float LocalT = XMVectorGetX(XMVector3Dot(ToHit, LocalDirection));
	return (LocalT > 0.0f) ? LocalT : 0.0f;
}

bool BuildLocalPickRay(
	const UPrimitiveComponent* InPrimitive,
	const FWorldRay& InWorldRay,
	FWorldRay* OutLocalRay,
	DirectX::XMMATRIX* OutWorldMatrix)
{
	if (InPrimitive == nullptr || OutLocalRay == nullptr || OutWorldMatrix == nullptr)
	{
		return false;
	}

	using namespace DirectX;
	*OutWorldMatrix = InPrimitive->GetWorldTransform().ToMatrix();
	const XMVECTOR Determinant = XMMatrixDeterminant(*OutWorldMatrix);
	if (std::fabs(XMVectorGetX(Determinant)) < 1e-8f)
	{
		return false;
	}
	const XMMATRIX InvWorldMatrix = XMMatrixInverse(nullptr, *OutWorldMatrix);

	const XMVECTOR WorldOrigin = XMVectorSet(
		InWorldRay.Origin.X,
		InWorldRay.Origin.Y,
		InWorldRay.Origin.Z,
		1.0f);
	const XMVECTOR WorldDirection = XMVectorSet(
		InWorldRay.Direction.X,
		InWorldRay.Direction.Y,
		InWorldRay.Direction.Z,
		0.0f);
	const XMVECTOR LocalOrigin = XMVector3TransformCoord(WorldOrigin, InvWorldMatrix);
	XMVECTOR LocalDirection = XMVector3TransformNormal(WorldDirection, InvWorldMatrix);
	const float DirectionLength = XMVectorGetX(XMVector3Length(LocalDirection));
	if (DirectionLength < 1e-6f)
	{
		return false;
	}
	LocalDirection = XMVectorScale(LocalDirection, 1.0f / DirectionLength);

	OutLocalRay->Origin = FVector3(
		XMVectorGetX(LocalOrigin),
		XMVectorGetY(LocalOrigin),
		XMVectorGetZ(LocalOrigin));
	OutLocalRay->Direction = FVector3(
		XMVectorGetX(LocalDirection),
		XMVectorGetY(LocalDirection),
		XMVectorGetZ(LocalDirection));
	return true;
}

bool TryPickWorldObb(
	const FWorldRay& InRay,
	FEditorOrientedBox InWorldObb,
	const FVector3& InCameraPosition,
	float* InOutBestT,
	uint64_t InActorObjectId,
	uint64_t* InOutBestActorId)
{
	ApplyPickObbMinimumSize(&InWorldObb, InCameraPosition);
	float EnterT = 0.0f;
	if (!FEditorRayIntersection::RayIntersectsObb(InRay, InWorldObb, &EnterT, nullptr))
	{
		return false;
	}

	const float HitT = (EnterT >= 0.0f) ? EnterT : 0.0f;
	return TryUpdateBestPickHit(HitT, InActorObjectId, InOutBestT, InOutBestActorId);
}

bool TryPickStaticMeshTriangles(
	const FWorldRay& InRay,
	const UStaticMeshComponent* InStaticMeshComponent,
	const FVector3& InCameraPosition,
	EPickTriangleBvhSplitMethod InSplitMethod,
	uint64_t InActorObjectId,
	float* InOutBestT,
	uint64_t* InOutBestActorId,
	bool* OutHasSectionBroadPhase)
{
	if (OutHasSectionBroadPhase != nullptr)
	{
		*OutHasSectionBroadPhase = false;
	}

	if (InStaticMeshComponent == nullptr || InOutBestT == nullptr || InOutBestActorId == nullptr)
	{
		return false;
	}
	(void)InCameraPosition;

	const UStaticMesh* StaticMesh = InStaticMeshComponent->GetStaticMesh();
	if (StaticMesh == nullptr
		|| StaticMesh->GetSectionCount() == 0
		|| StaticMesh->GetIndices().size() < 3)
	{
		return false;
	}

	const FEditorTriangleBvh* TriangleBvh = GTriangleBvhCache.GetOrBuild(StaticMesh, InSplitMethod);
	if (TriangleBvh == nullptr || !TriangleBvh->IsBuilt())
	{
		return false;
	}

	FWorldRay LocalRay{};
	DirectX::XMMATRIX WorldMatrix = DirectX::XMMatrixIdentity();
	if (!BuildLocalPickRay(InStaticMeshComponent, InRay, &LocalRay, &WorldMatrix))
	{
		return false;
	}

	bool bHasValidSectionBounds = false;
	for (size_t SectionIndex = 0; SectionIndex < StaticMesh->GetSectionCount(); ++SectionIndex)
	{
		const UStaticMesh::FStaticMeshSection& Section = StaticMesh->GetSection(SectionIndex);
		const UPrimitiveComponent::FBounds SectionLocalBounds = ToPrimitiveBounds(Section.SectionBounds);
		if (HasValidBounds(SectionLocalBounds))
		{
			bHasValidSectionBounds = true;
			break;
		}
	}

	const float LocalMaxT = ComputeLocalPickMaxTFromWorldBestT(
		InRay,
		WorldMatrix,
		LocalRay,
		*InOutBestT);
	const uint32_t FullIndexCount = static_cast<uint32_t>(StaticMesh->GetIndices().size());

	FEditorTriangleBvhQueryResult TriangleResult{};
	TriangleBvh->Query(LocalRay, 0, FullIndexCount, LocalMaxT, &TriangleResult);
	if (!TriangleResult.bHit)
	{
		if (OutHasSectionBroadPhase != nullptr && bHasValidSectionBounds)
		{
			*OutHasSectionBroadPhase = true;
		}
		return false;
	}

	const float WorldHitT = ComputeWorldRayDistanceFromLocalHit(
		InRay,
		WorldMatrix,
		LocalRay,
		TriangleResult.HitT);
	if (WorldHitT < 0.0f)
	{
		if (OutHasSectionBroadPhase != nullptr && bHasValidSectionBounds)
		{
			*OutHasSectionBroadPhase = true;
		}
		return false;
	}

	const bool bHit = TryUpdateBestPickHit(WorldHitT, InActorObjectId, InOutBestT, InOutBestActorId);
	if (OutHasSectionBroadPhase != nullptr && bHasValidSectionBounds)
	{
		*OutHasSectionBroadPhase = true;
	}
	return bHit;
}

bool TryPickStaticMeshSectionBounds(
	const FWorldRay& InRay,
	const UStaticMeshComponent* InStaticMeshComponent,
	const FVector3& InCameraPosition,
	EPickTriangleBvhSplitMethod InSplitMethod,
	uint64_t InActorObjectId,
	float* InOutBestT,
	uint64_t* InOutBestActorId,
	bool* OutHasSectionBroadPhase)
{
	return TryPickStaticMeshTriangles(
		InRay,
		InStaticMeshComponent,
		InCameraPosition,
		InSplitMethod,
		InActorObjectId,
		InOutBestT,
		InOutBestActorId,
		OutHasSectionBroadPhase);
}

bool TryPickSkeletalMeshTriangles(
	const FWorldRay& InRay,
	const USkeletalMeshComponent* InSkeletalMeshComponent,
	EPickTriangleBvhSplitMethod InSplitMethod,
	uint64_t InActorObjectId,
	float* InOutBestT,
	uint64_t* InOutBestActorId,
	bool* OutHasTriangleGeometry)
{
	if (OutHasTriangleGeometry != nullptr)
	{
		*OutHasTriangleGeometry = false;
	}

	if (InSkeletalMeshComponent == nullptr || InOutBestT == nullptr || InOutBestActorId == nullptr)
	{
		return false;
	}

	const USkeletalMesh* SkeletalMesh = InSkeletalMeshComponent->GetSkeletalMesh();
	if (SkeletalMesh == nullptr
		|| SkeletalMesh->GetSkinVertices().empty()
		|| SkeletalMesh->GetIndices().size() < 3
		|| SkeletalMesh->GetSectionCount() == 0)
	{
		return false;
	}

	if (OutHasTriangleGeometry != nullptr)
	{
		*OutHasTriangleGeometry = true;
	}

	const FEditorTriangleBvh* TriangleBvh = GTriangleBvhCache.GetOrBuild(SkeletalMesh, InSplitMethod);
	if (TriangleBvh == nullptr || !TriangleBvh->IsBuilt())
	{
		return false;
	}

	FWorldRay LocalRay{};
	DirectX::XMMATRIX WorldMatrix = DirectX::XMMatrixIdentity();
	if (!BuildLocalPickRay(InSkeletalMeshComponent, InRay, &LocalRay, &WorldMatrix))
	{
		return false;
	}

	const float LocalMaxT = ComputeLocalPickMaxTFromWorldBestT(
		InRay,
		WorldMatrix,
		LocalRay,
		*InOutBestT);
	const uint32_t FullIndexCount = static_cast<uint32_t>(SkeletalMesh->GetIndices().size());

	FEditorTriangleBvhQueryResult TriangleResult{};
	TriangleBvh->Query(LocalRay, 0, FullIndexCount, LocalMaxT, &TriangleResult);
	if (!TriangleResult.bHit)
	{
		return false;
	}

	const float WorldHitT = ComputeWorldRayDistanceFromLocalHit(
		InRay,
		WorldMatrix,
		LocalRay,
		TriangleResult.HitT);
	if (WorldHitT < 0.0f)
	{
		return false;
	}

	return TryUpdateBestPickHit(WorldHitT, InActorObjectId, InOutBestT, InOutBestActorId);
}

bool TryPickPrimitiveBounds(
	const FWorldRay& InRay,
	const UPrimitiveComponent* InPrimitive,
	const FVector3& InCameraPosition,
	EPickTriangleBvhSplitMethod InSplitMethod,
	uint64_t InActorObjectId,
	float* InOutBestT,
	uint64_t* InOutBestActorId)
{
	if (InPrimitive == nullptr || InOutBestT == nullptr || InOutBestActorId == nullptr)
	{
		return false;
	}

	const UPrimitiveComponent::FBounds LocalBounds = GetPrimitivePickLocalBounds(InPrimitive);
	if (!HasValidBounds(LocalBounds))
	{
		return false;
	}

	const FEditorOrientedBox WorldObb = BuildPrimitiveWorldObb(InPrimitive, LocalBounds);
	if (IsPrimitiveBeyondCullDistance(InPrimitive, WorldObb, InCameraPosition))
	{
		return false;
	}

	if (const UStaticMeshComponent* StaticMeshComponent =
			dynamic_cast<const UStaticMeshComponent*>(InPrimitive))
	{
		bool bHasSectionBroadPhase = false;
		const bool bHitSection = TryPickStaticMeshSectionBounds(
			InRay,
			StaticMeshComponent,
			InCameraPosition,
			InSplitMethod,
			InActorObjectId,
			InOutBestT,
			InOutBestActorId,
			&bHasSectionBroadPhase);
		if (bHasSectionBroadPhase)
		{
			return bHitSection;
		}
	}
	else if (const USkeletalMeshComponent* SkeletalMeshComponent =
			dynamic_cast<const USkeletalMeshComponent*>(InPrimitive))
	{
		bool bHasTriangleGeometry = false;
		const bool bHitSkeletalTriangles = TryPickSkeletalMeshTriangles(
			InRay,
			SkeletalMeshComponent,
			InSplitMethod,
			InActorObjectId,
			InOutBestT,
			InOutBestActorId,
			&bHasTriangleGeometry);
		if (bHasTriangleGeometry)
		{
			return bHitSkeletalTriangles;
		}
	}

	return TryPickWorldObb(
		InRay,
		WorldObb,
		InCameraPosition,
		InOutBestT,
		InActorObjectId,
		InOutBestActorId);
}

bool TryPickActorDefaultBounds(
	const FWorldRay& InRay,
	const AActor* InActor,
	float* InOutBestT,
	uint64_t* InOutBestActorId)
{
	if (InActor == nullptr || InOutBestT == nullptr || InOutBestActorId == nullptr)
	{
		return false;
	}

	const FVector3 Center = InActor->GetActorTransform().Position;
	FEditorAxisAlignedBox DefaultBox{};
	DefaultBox.Min = Center - FVector3(kDefaultBoundsRadius, kDefaultBoundsRadius, kDefaultBoundsRadius);
	DefaultBox.Max = Center + FVector3(kDefaultBoundsRadius, kDefaultBoundsRadius, kDefaultBoundsRadius);

	float EnterT = 0.0f;
	if (!FEditorRayIntersection::RayIntersectsAabb(InRay, DefaultBox, &EnterT, nullptr))
	{
		return false;
	}

	const float HitT = (EnterT >= 0.0f) ? EnterT : 0.0f;
	return TryUpdateBestPickHit(HitT, InActor->GetObjectId(), InOutBestT, InOutBestActorId);
}

void TryPickActorDetailed(
	const FWorldRay& InRay,
	const AActor* InActor,
	const FVector3& InCameraPosition,
	EPickTriangleBvhSplitMethod InSplitMethod,
	uint64_t InActorObjectId,
	float* InOutBestT,
	uint64_t* InOutBestActorId)
{
	if (InActor == nullptr || InOutBestT == nullptr || InOutBestActorId == nullptr)
	{
		return;
	}

	bool bHasVisiblePickBounds = false;
	for (UActorComponent* Component : InActor->GetComponents())
	{
		UPrimitiveComponent* Primitive = dynamic_cast<UPrimitiveComponent*>(Component);
		if (Primitive == nullptr || !Primitive->IsVisible())
		{
			continue;
		}

		const UPrimitiveComponent::FBounds LocalBounds = GetPrimitivePickLocalBounds(Primitive);
		if (!HasValidBounds(LocalBounds))
		{
			continue;
		}

		bHasVisiblePickBounds = true;
		TryPickPrimitiveBounds(
			InRay,
			Primitive,
			InCameraPosition,
			InSplitMethod,
			InActorObjectId,
			InOutBestT,
			InOutBestActorId);
	}

	if (!bHasVisiblePickBounds)
	{
		TryPickActorDefaultBounds(InRay, InActor, InOutBestT, InOutBestActorId);
	}
}

void CollectActorsForPick(
	const ULevel* InLevel,
	const std::vector<FEditorPickBvhCandidate>& InBvhCandidates,
	std::vector<uint64_t>* OutActorObjectIds)
{
	if (InLevel == nullptr || OutActorObjectIds == nullptr)
	{
		return;
	}

	OutActorObjectIds->clear();
	std::unordered_set<uint64_t> SeenActorIds;
	OutActorObjectIds->reserve(InLevel->GetActorCount());

	auto AddActorId = [&](uint64_t InActorObjectId)
	{
		if (SeenActorIds.insert(InActorObjectId).second)
		{
			OutActorObjectIds->push_back(InActorObjectId);
		}
	};

	if (!InBvhCandidates.empty())
	{
		for (const FEditorPickBvhCandidate& Candidate : InBvhCandidates)
		{
			AddActorId(Candidate.ActorObjectId);
		}
		return;
	}

	for (uint64_t ActorObjectId : InLevel->GetActorObjectIds())
	{
		AddActorId(ActorObjectId);
	}
}

struct FEditorPickBvhCache
{
	const ULevel* CachedLevel = nullptr;
	uint32_t CachedSceneRevision = 0;
	FEditorPickBvh Bvh;
};

FEditorPickBvhCache GPickBvhCache;

void EnsurePickBvhBuilt(const ULevel* InLevel, uint32_t InSceneRevision)
{
	if (InLevel == nullptr)
	{
		GPickBvhCache.CachedLevel = nullptr;
		GPickBvhCache.CachedSceneRevision = 0;
		GPickBvhCache.Bvh.Clear();
		return;
	}

	if (GPickBvhCache.CachedLevel == InLevel
		&& GPickBvhCache.CachedSceneRevision == InSceneRevision
		&& GPickBvhCache.Bvh.IsBuilt())
	{
		return;
	}

	GPickBvhCache.Bvh.Build(InLevel);
	GPickBvhCache.CachedLevel = InLevel;
	GPickBvhCache.CachedSceneRevision = InSceneRevision;
}
} // namespace

void FEditorPicking::InvalidateTriangleBvhCache()
{
	GTriangleBvhCache.InvalidateAll();
}

FEditorPickResult FEditorPicking::PickActor(
	const ULevel* InLevel,
	const Dx12Renderer::CameraState& InCamera,
	UINT InViewportWidth,
	UINT InViewportHeight,
	float InNearPlane,
	float InFarPlane,
	float InScreenX,
	float InScreenY,
	EPickTriangleBvhSplitMethod InTriangleBvhSplitMethod,
	uint32_t InSceneRevision)
{
	FEditorPickResult Result;
	if (InLevel == nullptr || InViewportWidth == 0 || InViewportHeight == 0)
	{
		return Result;
	}

	const FEditorViewportMatrices Matrices = FEditorViewMatrices::Build(
		InCamera, InViewportWidth, InViewportHeight, InNearPlane, InFarPlane);
	const FWorldRay Ray = FEditorViewMatrices::BuildWorldRayFromScreen(
		Matrices,
		InCamera.position,
		InScreenX,
		InScreenY,
		InViewportWidth,
		InViewportHeight,
		InNearPlane,
		InFarPlane);
	const FVector3 CameraPosition(
		InCamera.position.x,
		InCamera.position.y,
		InCamera.position.z);

	EnsurePickBvhBuilt(InLevel, InSceneRevision);

	float BestT = std::numeric_limits<float>::max();
	uint64_t BestActorId = 0;

	std::vector<FEditorPickBvhCandidate> BvhCandidates;
	std::vector<uint64_t> ActorObjectIdsToTest;
	if (GPickBvhCache.Bvh.IsBuilt())
	{
		GPickBvhCache.Bvh.Query(Ray, BestT, &BvhCandidates);
	}
	CollectActorsForPick(InLevel, BvhCandidates, &ActorObjectIdsToTest);

	for (uint64_t ActorObjectId : ActorObjectIdsToTest)
	{
		const AActor* Actor = InLevel->FindActor(ActorObjectId);
		if (Actor == nullptr || Actor->IsPendingDestroy() || !Actor->IsPickable())
		{
			continue;
		}

		TryPickActorDetailed(
			Ray,
			Actor,
			CameraPosition,
			InTriangleBvhSplitMethod,
			ActorObjectId,
			&BestT,
			&BestActorId);
	}

	if (BestActorId != 0)
	{
		Result.bHit = true;
		Result.ActorObjectId = BestActorId;
		Result.HitDistance = BestT;
	}

	return Result;
}
