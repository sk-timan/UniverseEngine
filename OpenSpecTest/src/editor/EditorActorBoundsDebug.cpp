#include "editor/EditorActorBoundsDebug.h"

#include <algorithm>

#include <DirectXMath.h>

#include "components/MeshComponent.h"
#include "components/PrimitiveComponent.h"
#include "components/StaticMeshComponent.h"
#include "math/FTransform.h"
#include "render/asset/StaticMesh.h"
#include "world/Actor.h"

namespace
{
constexpr float kDefaultBoundsRadius = 2.0f;

struct FAabb
{
	FVector3 Min{};
	FVector3 Max{};
};

void ExpandAabbWithPoint(FAabb& InOutBox, const FVector3& InPoint)
{
	InOutBox.Min.X = std::min(InOutBox.Min.X, InPoint.X);
	InOutBox.Min.Y = std::min(InOutBox.Min.Y, InPoint.Y);
	InOutBox.Min.Z = std::min(InOutBox.Min.Z, InPoint.Z);
	InOutBox.Max.X = std::max(InOutBox.Max.X, InPoint.X);
	InOutBox.Max.Y = std::max(InOutBox.Max.Y, InPoint.Y);
	InOutBox.Max.Z = std::max(InOutBox.Max.Z, InPoint.Z);
}

FAabb BuildWorldAabbFromLocalBounds(const FTransform& InWorldTransform, const FVector3& InOrigin, const FVector3& InExtent)
{
	FAabb Result;
	bool bHasPoint = false;
	const FVector3 Corners[2] = {
		{InOrigin.X - InExtent.X, InOrigin.Y - InExtent.Y, InOrigin.Z - InExtent.Z},
		{InOrigin.X + InExtent.X, InOrigin.Y + InExtent.Y, InOrigin.Z + InExtent.Z},
	};

	for (int X = 0; X < 2; ++X)
	{
		for (int Y = 0; Y < 2; ++Y)
		{
			for (int Z = 0; Z < 2; ++Z)
			{
				const FVector3 LocalCorner(
					(X == 0) ? Corners[0].X : Corners[1].X,
					(Y == 0) ? Corners[0].Y : Corners[1].Y,
					(Z == 0) ? Corners[0].Z : Corners[1].Z);
				const DirectX::XMVECTOR WorldCorner = DirectX::XMVector3Transform(
					DirectX::XMVectorSet(LocalCorner.X, LocalCorner.Y, LocalCorner.Z, 1.0f),
					InWorldTransform.ToMatrix());
				const FVector3 WorldPoint(
					DirectX::XMVectorGetX(WorldCorner),
					DirectX::XMVectorGetY(WorldCorner),
					DirectX::XMVectorGetZ(WorldCorner));
				if (!bHasPoint)
				{
					Result.Min = WorldPoint;
					Result.Max = WorldPoint;
					bHasPoint = true;
				}
				else
				{
					ExpandAabbWithPoint(Result, WorldPoint);
				}
			}
		}
	}
	return Result;
}

void AppendLine(
	std::vector<Dx12Renderer::Vertex>* OutVertices,
	const FVector3& InStart,
	const FVector3& InEnd,
	const DirectX::XMFLOAT4& InColor)
{
	if (OutVertices == nullptr)
	{
		return;
	}

	const DirectX::XMFLOAT3 Normal{0.0f, 0.0f, 1.0f};
	OutVertices->push_back(
		{{InStart.X, InStart.Y, InStart.Z}, Normal, InColor});
	OutVertices->push_back(
		{{InEnd.X, InEnd.Y, InEnd.Z}, Normal, InColor});
}

bool HasValidLocalBounds(const UPrimitiveComponent::FBounds& InBounds)
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

UPrimitiveComponent::FBounds ToPrimitiveBounds(const UStreamableRenderAsset::FBounds& InBounds)
{
	return {InBounds.Origin, InBounds.Extent, InBounds.SphereRadius};
}

FEditorWorldObb BuildPrimitiveWorldObb(
	const UPrimitiveComponent* InPrimitive,
	const UPrimitiveComponent::FBounds& InLocalBounds)
{
	FEditorWorldObb Result{};
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
	Result.bIsValid = true;
	return Result;
}

FVector3 BuildObbCorner(
	const FEditorWorldObb& InObb,
	float InSignX,
	float InSignY,
	float InSignZ)
{
	return InObb.Center
		+ InObb.AxisX * (InObb.HalfExtents.X * InSignX)
		+ InObb.AxisY * (InObb.HalfExtents.Y * InSignY)
		+ InObb.AxisZ * (InObb.HalfExtents.Z * InSignZ);
}
} // namespace

bool FEditorActorBoundsDebug::ComputeActorWorldAabb(const AActor* InActor, FEditorWorldAabb* OutAabb)
{
	if (InActor == nullptr || OutAabb == nullptr)
	{
		return false;
	}

	OutAabb->bIsValid = false;
	FAabb MergedBox{};
	bool bHasBounds = false;

	for (UActorComponent* Component : InActor->GetComponents())
	{
		UPrimitiveComponent* Primitive = dynamic_cast<UPrimitiveComponent*>(Component);
		if (Primitive == nullptr)
		{
			continue;
		}

		UPrimitiveComponent::FBounds LocalBounds = Primitive->GetBounds();
		if (UMeshComponent* MeshComponent = dynamic_cast<UMeshComponent*>(Primitive))
		{
			LocalBounds = MeshComponent->GetBounds();
		}
		if (LocalBounds.Extent.X <= 0.0f && LocalBounds.Extent.Y <= 0.0f && LocalBounds.Extent.Z <= 0.0f)
		{
			continue;
		}

		const FAabb WorldBox = BuildWorldAabbFromLocalBounds(
			Primitive->GetWorldTransform(),
			LocalBounds.Origin,
			LocalBounds.Extent);
		if (!bHasBounds)
		{
			MergedBox = WorldBox;
			bHasBounds = true;
		}
		else
		{
			ExpandAabbWithPoint(MergedBox, WorldBox.Min);
			ExpandAabbWithPoint(MergedBox, WorldBox.Max);
		}
	}

	if (!bHasBounds)
	{
		const FVector3 Center = InActor->GetActorTransform().Position;
		MergedBox.Min = Center - FVector3(kDefaultBoundsRadius, kDefaultBoundsRadius, kDefaultBoundsRadius);
		MergedBox.Max = Center + FVector3(kDefaultBoundsRadius, kDefaultBoundsRadius, kDefaultBoundsRadius);
		bHasBounds = true;
	}

	if (bHasBounds)
	{
		OutAabb->Min = MergedBox.Min;
		OutAabb->Max = MergedBox.Max;
		OutAabb->bIsValid = true;
	}
	return OutAabb->bIsValid;
}

void FEditorActorBoundsDebug::AppendWorldAabbWireframe(
	const FEditorWorldAabb& InAabb,
	const DirectX::XMFLOAT4& InColor,
	std::vector<Dx12Renderer::Vertex>* OutVertices)
{
	if (OutVertices == nullptr || !InAabb.bIsValid)
	{
		return;
	}

	const FVector3 Corners[8] = {
		{InAabb.Min.X, InAabb.Min.Y, InAabb.Min.Z},
		{InAabb.Max.X, InAabb.Min.Y, InAabb.Min.Z},
		{InAabb.Min.X, InAabb.Max.Y, InAabb.Min.Z},
		{InAabb.Max.X, InAabb.Max.Y, InAabb.Min.Z},
		{InAabb.Min.X, InAabb.Min.Y, InAabb.Max.Z},
		{InAabb.Max.X, InAabb.Min.Y, InAabb.Max.Z},
		{InAabb.Min.X, InAabb.Max.Y, InAabb.Max.Z},
		{InAabb.Max.X, InAabb.Max.Y, InAabb.Max.Z},
	};

	const int EdgePairs[12][2] = {
		{0, 1}, {1, 3}, {3, 2}, {2, 0},
		{4, 5}, {5, 7}, {7, 6}, {6, 4},
		{0, 4}, {1, 5}, {2, 6}, {3, 7},
	};

	for (const auto& Edge : EdgePairs)
	{
		AppendLine(OutVertices, Corners[Edge[0]], Corners[Edge[1]], InColor);
	}
}

void FEditorActorBoundsDebug::AppendWorldObbWireframe(
	const FEditorWorldObb& InObb,
	const DirectX::XMFLOAT4& InColor,
	std::vector<Dx12Renderer::Vertex>* OutVertices)
{
	if (OutVertices == nullptr || !InObb.bIsValid)
	{
		return;
	}

	const FVector3 Corners[8] = {
		BuildObbCorner(InObb, -1.0f, -1.0f, -1.0f),
		BuildObbCorner(InObb, 1.0f, -1.0f, -1.0f),
		BuildObbCorner(InObb, -1.0f, 1.0f, -1.0f),
		BuildObbCorner(InObb, 1.0f, 1.0f, -1.0f),
		BuildObbCorner(InObb, -1.0f, -1.0f, 1.0f),
		BuildObbCorner(InObb, 1.0f, -1.0f, 1.0f),
		BuildObbCorner(InObb, -1.0f, 1.0f, 1.0f),
		BuildObbCorner(InObb, 1.0f, 1.0f, 1.0f),
	};

	const int EdgePairs[12][2] = {
		{0, 1}, {1, 3}, {3, 2}, {2, 0},
		{4, 5}, {5, 7}, {7, 6}, {6, 4},
		{0, 4}, {1, 5}, {2, 6}, {3, 7},
	};

	for (const auto& Edge : EdgePairs)
	{
		AppendLine(OutVertices, Corners[Edge[0]], Corners[Edge[1]], InColor);
	}
}

void FEditorActorBoundsDebug::AppendActorWorldObbWireframes(
	const AActor* InActor,
	const DirectX::XMFLOAT4& InColor,
	std::vector<Dx12Renderer::Vertex>* OutVertices)
{
	if (InActor == nullptr || OutVertices == nullptr)
	{
		return;
	}

	bool bHasBounds = false;
	for (UActorComponent* Component : InActor->GetComponents())
	{
		UPrimitiveComponent* Primitive = dynamic_cast<UPrimitiveComponent*>(Component);
		if (Primitive == nullptr)
		{
			continue;
		}

		const UPrimitiveComponent::FBounds LocalBounds = GetPrimitiveLocalBounds(Primitive);
		if (!HasValidLocalBounds(LocalBounds))
		{
			continue;
		}

		bHasBounds = true;
		const FEditorWorldObb WorldObb = BuildPrimitiveWorldObb(Primitive, LocalBounds);
		AppendWorldObbWireframe(WorldObb, InColor, OutVertices);
	}

	if (!bHasBounds)
	{
		const FVector3 Center = InActor->GetActorTransform().Position;
		FEditorWorldObb DefaultObb{};
		DefaultObb.Center = Center;
		DefaultObb.AxisX = FVector3(1.0f, 0.0f, 0.0f);
		DefaultObb.AxisY = FVector3(0.0f, 1.0f, 0.0f);
		DefaultObb.AxisZ = FVector3(0.0f, 0.0f, 1.0f);
		DefaultObb.HalfExtents = FVector3(kDefaultBoundsRadius, kDefaultBoundsRadius, kDefaultBoundsRadius);
		DefaultObb.bIsValid = true;
		AppendWorldObbWireframe(DefaultObb, InColor, OutVertices);
	}
}

void FEditorActorBoundsDebug::AppendActorWorldSectionBoundsWireframes(
	const AActor* InActor,
	const DirectX::XMFLOAT4& InColor,
	std::vector<Dx12Renderer::Vertex>* OutVertices)
{
	if (InActor == nullptr || OutVertices == nullptr)
	{
		return;
	}

	for (UActorComponent* Component : InActor->GetComponents())
	{
		const UStaticMeshComponent* StaticMeshComponent = dynamic_cast<const UStaticMeshComponent*>(Component);
		if (StaticMeshComponent == nullptr)
		{
			continue;
		}

		const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
		if (StaticMesh == nullptr)
		{
			continue;
		}

		for (size_t SectionIndex = 0; SectionIndex < StaticMesh->GetSectionCount(); ++SectionIndex)
		{
			const UStaticMesh::FStaticMeshSection& Section = StaticMesh->GetSection(SectionIndex);
			const UPrimitiveComponent::FBounds SectionLocalBounds = ToPrimitiveBounds(Section.SectionBounds);
			if (!HasValidLocalBounds(SectionLocalBounds))
			{
				continue;
			}

			const FEditorWorldObb SectionWorldObb =
				BuildPrimitiveWorldObb(StaticMeshComponent, SectionLocalBounds);
			AppendWorldObbWireframe(SectionWorldObb, InColor, OutVertices);
		}
	}
}
