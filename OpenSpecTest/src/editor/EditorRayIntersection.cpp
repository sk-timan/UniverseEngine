#include "editor/EditorRayIntersection.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace DirectX;

namespace
{
constexpr float kRayEpsilon = 1e-5f;

bool RayIntersectsSlabAabb(
	const FVector3& InRayOrigin,
	const FVector3& InRayDirection,
	const FVector3& InBoxMin,
	const FVector3& InBoxMax,
	float* OutEnterT,
	float* OutExitT)
{
	float TMin = 0.0f;
	float TMax = std::numeric_limits<float>::max();

	auto Slab = [&](float InOrigin, float InDir, float InMin, float InMax) -> bool
	{
		if (std::fabs(InDir) < kRayEpsilon)
		{
			return InOrigin >= InMin && InOrigin <= InMax;
		}

		const float InvD = 1.0f / InDir;
		float T0 = (InMin - InOrigin) * InvD;
		float T1 = (InMax - InOrigin) * InvD;
		if (T0 > T1)
		{
			std::swap(T0, T1);
		}
		TMin = std::max(TMin, T0);
		TMax = std::min(TMax, T1);
		return TMax >= TMin;
	};

	if (!Slab(InRayOrigin.X, InRayDirection.X, InBoxMin.X, InBoxMax.X))
	{
		return false;
	}
	if (!Slab(InRayOrigin.Y, InRayDirection.Y, InBoxMin.Y, InBoxMax.Y))
	{
		return false;
	}
	if (!Slab(InRayOrigin.Z, InRayDirection.Z, InBoxMin.Z, InBoxMax.Z))
	{
		return false;
	}

	if (OutEnterT != nullptr)
	{
		*OutEnterT = TMin;
	}
	if (OutExitT != nullptr)
	{
		*OutExitT = TMax;
	}
	return TMax >= 0.0f;
}

void ExpandAabbWithPoint(FEditorAxisAlignedBox& InOutBox, const FVector3& InPoint)
{
	InOutBox.Min.X = std::min(InOutBox.Min.X, InPoint.X);
	InOutBox.Min.Y = std::min(InOutBox.Min.Y, InPoint.Y);
	InOutBox.Min.Z = std::min(InOutBox.Min.Z, InPoint.Z);
	InOutBox.Max.X = std::max(InOutBox.Max.X, InPoint.X);
	InOutBox.Max.Y = std::max(InOutBox.Max.Y, InPoint.Y);
	InOutBox.Max.Z = std::max(InOutBox.Max.Z, InPoint.Z);
}

FVector3 BuildObbCorner(
	const FEditorOrientedBox& InObb,
	float InSignX,
	float InSignY,
	float InSignZ)
{
	return InObb.Center
		+ InObb.AxisX * (InObb.HalfExtents.X * InSignX)
		+ InObb.AxisY * (InObb.HalfExtents.Y * InSignY)
		+ InObb.AxisZ * (InObb.HalfExtents.Z * InSignZ);
}

void NormalizePlane(FVector3* InOutNormal, float* InOutDistance)
{
	const float Length = InOutNormal->Length();
	if (Length < kRayEpsilon)
	{
		InOutNormal->X = 0.0f;
		InOutNormal->Y = 0.0f;
		InOutNormal->Z = 1.0f;
		*InOutDistance = 0.0f;
		return;
	}

	const float InvLength = 1.0f / Length;
	*InOutNormal = *InOutNormal * InvLength;
	*InOutDistance *= InvLength;
}
} // namespace

bool FEditorRayIntersection::RayIntersectsTriangle(
	const FWorldRay& InRay,
	const FVector3& InV0,
	const FVector3& InV1,
	const FVector3& InV2,
	float InMaxT,
	FRayTriangleHit* OutHit,
	bool bDoubleSided)
{
	if (OutHit == nullptr)
	{
		return false;
	}

	*OutHit = FRayTriangleHit{};

	const FVector3 Edge1 = InV1 - InV0;
	const FVector3 Edge2 = InV2 - InV0;
	const FVector3 Pvec = InRay.Direction.Cross(Edge2);
	const float Determinant = Edge1.Dot(Pvec);
	if (std::fabs(Determinant) < kRayEpsilon)
	{
		return false;
	}

	const float InvDeterminant = 1.0f / Determinant;
	const FVector3 Tvec = InRay.Origin - InV0;
	const float U = Tvec.Dot(Pvec) * InvDeterminant;
	if (U < 0.0f || U > 1.0f)
	{
		return false;
	}

	const FVector3 Qvec = Tvec.Cross(Edge1);
	const float V = InRay.Direction.Dot(Qvec) * InvDeterminant;
	if (V < 0.0f || U + V > 1.0f)
	{
		return false;
	}

	const float T = Edge2.Dot(Qvec) * InvDeterminant;
	if (!bDoubleSided)
	{
		if (T < kRayEpsilon || T > InMaxT)
		{
			return false;
		}
	}
	else
	{
		const float SignedT = (Determinant < 0.0f) ? -T : T;
		if (SignedT < kRayEpsilon || SignedT > InMaxT)
		{
			return false;
		}
		OutHit->T = SignedT;
		OutHit->U = U;
		OutHit->V = V;
		OutHit->bHit = true;
		return true;
	}

	OutHit->T = T;
	OutHit->U = U;
	OutHit->V = V;
	OutHit->bHit = true;
	return true;
}

bool FEditorRayIntersection::RayIntersectsAabb(
	const FWorldRay& InRay,
	const FEditorAxisAlignedBox& InBox,
	float* OutEnterT,
	float* OutExitT)
{
	return RayIntersectsSlabAabb(
		InRay.Origin,
		InRay.Direction,
		InBox.Min,
		InBox.Max,
		OutEnterT,
		OutExitT);
}

bool FEditorRayIntersection::RayIntersectsObb(
	const FWorldRay& InRay,
	const FEditorOrientedBox& InBox,
	float* OutEnterT,
	float* OutExitT)
{
	const FVector3 RelativeOrigin = InRay.Origin - InBox.Center;
	const FVector3 LocalOrigin(
		RelativeOrigin.Dot(InBox.AxisX),
		RelativeOrigin.Dot(InBox.AxisY),
		RelativeOrigin.Dot(InBox.AxisZ));
	const FVector3 LocalDirection(
		InRay.Direction.Dot(InBox.AxisX),
		InRay.Direction.Dot(InBox.AxisY),
		InRay.Direction.Dot(InBox.AxisZ));
	const FVector3 BoxMin(
		-InBox.HalfExtents.X,
		-InBox.HalfExtents.Y,
		-InBox.HalfExtents.Z);
	const FVector3 BoxMax(
		InBox.HalfExtents.X,
		InBox.HalfExtents.Y,
		InBox.HalfExtents.Z);
	return RayIntersectsSlabAabb(LocalOrigin, LocalDirection, BoxMin, BoxMax, OutEnterT, OutExitT);
}

FEditorFrustumPlanes FEditorRayIntersection::BuildFrustumPlanes(const XMMATRIX& InViewProjectionMatrix)
{
	FEditorFrustumPlanes Result{};
	const XMMATRIX TransposedMatrix = XMMatrixTranspose(InViewProjectionMatrix);
	XMFLOAT4 Rows[4]{};
	XMStoreFloat4(&Rows[0], TransposedMatrix.r[0]);
	XMStoreFloat4(&Rows[1], TransposedMatrix.r[1]);
	XMStoreFloat4(&Rows[2], TransposedMatrix.r[2]);
	XMStoreFloat4(&Rows[3], TransposedMatrix.r[3]);

	const XMFLOAT4 PlaneRows[6] = {
		{Rows[0].x + Rows[3].x, Rows[0].y + Rows[3].y, Rows[0].z + Rows[3].z, Rows[0].w + Rows[3].w},
		{Rows[3].x - Rows[0].x, Rows[3].y - Rows[0].y, Rows[3].z - Rows[0].z, Rows[3].w - Rows[0].w},
		{Rows[1].x + Rows[3].x, Rows[1].y + Rows[3].y, Rows[1].z + Rows[3].z, Rows[1].w + Rows[3].w},
		{Rows[3].x - Rows[1].x, Rows[3].y - Rows[1].y, Rows[3].z - Rows[1].z, Rows[3].w - Rows[1].w},
		{Rows[2].x + Rows[3].x, Rows[2].y + Rows[3].y, Rows[2].z + Rows[3].z, Rows[2].w + Rows[3].w},
		{Rows[3].x - Rows[2].x, Rows[3].y - Rows[2].y, Rows[3].z - Rows[2].z, Rows[3].w - Rows[2].w},
	};

	for (int32_t PlaneIndex = 0; PlaneIndex < 6; ++PlaneIndex)
	{
		Result.Normals[PlaneIndex] = FVector3(
			PlaneRows[PlaneIndex].x,
			PlaneRows[PlaneIndex].y,
			PlaneRows[PlaneIndex].z);
		Result.Distances[PlaneIndex] = PlaneRows[PlaneIndex].w;
		NormalizePlane(&Result.Normals[PlaneIndex], &Result.Distances[PlaneIndex]);
	}
	return Result;
}

bool FEditorRayIntersection::IsAabbCompletelyOutsideFrustum(
	const FEditorAxisAlignedBox& InBox,
	const FEditorFrustumPlanes& InFrustum)
{
	for (int32_t PlaneIndex = 0; PlaneIndex < 6; ++PlaneIndex)
	{
		const FVector3& Normal = InFrustum.Normals[PlaneIndex];
		const FVector3 PositiveVertex(
			Normal.X >= 0.0f ? InBox.Max.X : InBox.Min.X,
			Normal.Y >= 0.0f ? InBox.Max.Y : InBox.Min.Y,
			Normal.Z >= 0.0f ? InBox.Max.Z : InBox.Min.Z);
		if (PositiveVertex.Dot(Normal) + InFrustum.Distances[PlaneIndex] < 0.0f)
		{
			return true;
		}
	}
	return false;
}

FEditorAxisAlignedBox FEditorRayIntersection::BuildWorldAabbFromObb(const FEditorOrientedBox& InObb)
{
	FEditorAxisAlignedBox Result{};
	bool bHasPoint = false;
	for (int32_t SignX = -1; SignX <= 1; SignX += 2)
	{
		for (int32_t SignY = -1; SignY <= 1; SignY += 2)
		{
			for (int32_t SignZ = -1; SignZ <= 1; SignZ += 2)
			{
				const FVector3 Corner = BuildObbCorner(
					InObb,
					static_cast<float>(SignX),
					static_cast<float>(SignY),
					static_cast<float>(SignZ));
				if (!bHasPoint)
				{
					Result.Min = Corner;
					Result.Max = Corner;
					bHasPoint = true;
				}
				else
				{
					ExpandAabbWithPoint(Result, Corner);
				}
			}
		}
	}
	return Result;
}
