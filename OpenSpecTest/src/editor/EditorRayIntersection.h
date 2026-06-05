#pragma once

#include <cstdint>

#include <DirectXMath.h>

#include "editor/EditorTypes.h"
#include "math/FVector3.h"

struct FEditorFrustumPlanes
{
	FVector3 Normals[6]{};
	float Distances[6]{};
};

struct FEditorOrientedBox
{
	FVector3 Center{};
	FVector3 AxisX{};
	FVector3 AxisY{};
	FVector3 AxisZ{};
	FVector3 HalfExtents{};
};

struct FEditorAxisAlignedBox
{
	FVector3 Min{};
	FVector3 Max{};
};

struct FRayTriangleHit
{
	bool bHit = false;
	float T = 0.0f;
	float U = 0.0f;
	float V = 0.0f;
};

class FEditorRayIntersection
{
public:
	static bool RayIntersectsTriangle(
		const FWorldRay& InRay,
		const FVector3& InV0,
		const FVector3& InV1,
		const FVector3& InV2,
		float InMaxT,
		FRayTriangleHit* OutHit,
		bool bDoubleSided = true);

	static bool RayIntersectsAabb(
		const FWorldRay& InRay,
		const FEditorAxisAlignedBox& InBox,
		float* OutEnterT,
		float* OutExitT = nullptr);

	static bool RayIntersectsObb(
		const FWorldRay& InRay,
		const FEditorOrientedBox& InBox,
		float* OutEnterT,
		float* OutExitT = nullptr);

	static FEditorFrustumPlanes BuildFrustumPlanes(const DirectX::XMMATRIX& InViewProjectionMatrix);

	static bool IsAabbCompletelyOutsideFrustum(
		const FEditorAxisAlignedBox& InBox,
		const FEditorFrustumPlanes& InFrustum);

	static FEditorAxisAlignedBox BuildWorldAabbFromObb(const FEditorOrientedBox& InObb);
};
