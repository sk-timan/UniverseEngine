#include "editor/EditorTransformGizmo.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include <DirectXMath.h>

#include "editor/EditorViewMatrices.h"
#include "math/FTransform.h"
#include "math/FRotator3.h"
#include "world/Actor.h"
#include "components/SceneComponent.h"

namespace
{
constexpr float kMinScaleValue = 0.001f;
constexpr float kGizmoScreenScaleFactor = 0.08f;
constexpr float kPi = 3.14159265f;
constexpr float kAxisCylinderRadiusFactor = 0.035f;
constexpr float kArrowLengthFactor = 0.22f;
constexpr float kArrowHeadRadiusFactor = 2.2f;
constexpr float kPlaneHandleSizeFactor = 0.22f;
constexpr float kCenterBoxHalfExtentFactor = 0.045f;

FVector3 RotateGizmoDirection(const FRotator3& InRotation, const FVector3& InLocalDirection)
{
	const FTransform Transform(FVector3{}, InRotation, FVector3{1.0f, 1.0f, 1.0f});
	const DirectX::XMVECTOR Local = DirectX::XMVectorSet(InLocalDirection.X, InLocalDirection.Y, InLocalDirection.Z, 0.0f);
	const DirectX::XMVECTOR World = DirectX::XMVector3Transform(Local, Transform.ToMatrix());
	DirectX::XMFLOAT3 Result{};
	DirectX::XMStoreFloat3(&Result, World);
	return FVector3{Result.x, Result.y, Result.z};
}

void GetLocalRingTangentAxes(EGizmoAxis InAxis, FVector3* OutTangentU, FVector3* OutTangentV)
{
	if (OutTangentU == nullptr || OutTangentV == nullptr)
	{
		return;
	}

	switch (InAxis)
	{
	case EGizmoAxis::X:
		*OutTangentU = FVector3{0.0f, 1.0f, 0.0f};
		*OutTangentV = FVector3{0.0f, 0.0f, 1.0f};
		break;
	case EGizmoAxis::Y:
		*OutTangentU = FVector3{1.0f, 0.0f, 0.0f};
		*OutTangentV = FVector3{0.0f, 0.0f, 1.0f};
		break;
	case EGizmoAxis::Z:
	default:
		*OutTangentU = FVector3{1.0f, 0.0f, 0.0f};
		*OutTangentV = FVector3{0.0f, 1.0f, 0.0f};
		break;
	}
}

float ComputeRotateRingAngleRadians(
	const FVector3& InWorldOffset,
	const FVector3& InTangentU,
	const FVector3& InTangentV)
{
	return std::atan2(InWorldOffset.Dot(InTangentV), InWorldOffset.Dot(InTangentU));
}

FVector3 BuildLocalRingPoint(EGizmoAxis InAxis, float InAngle, float InRadius)
{
	switch (InAxis)
	{
	case EGizmoAxis::X:
		return FVector3{0.0f, std::cos(InAngle) * InRadius, std::sin(InAngle) * InRadius};
	case EGizmoAxis::Y:
		return FVector3{std::cos(InAngle) * InRadius, 0.0f, std::sin(InAngle) * InRadius};
	case EGizmoAxis::Z:
	default:
		return FVector3{std::cos(InAngle) * InRadius, std::sin(InAngle) * InRadius, 0.0f};
	}
}
constexpr float kRotateTubeRadiusFactor = 0.04f;
constexpr float kHalfPi = kPi * 0.5f;
constexpr int32_t kCylinderSegments = 12;
constexpr int32_t kConeSegments = 12;
constexpr int32_t kRotateQuarterRingSegments = 16;
constexpr int32_t kRotateFullRingSegments = 48;
constexpr float kRotateDragSectorColorAlpha = 0.42f;
constexpr float kRotateDragTickRadiusFactor = 0.008f;
constexpr float kRotateDragTickExtentFactor = 0.045f;
constexpr float kRotateDragPointerSizeFactor = 0.055f;
constexpr float kScaleHandleHalfExtentFactor = 0.048f;
constexpr float kScaleConnectorRadiusFactor = 0.012f;
constexpr float kScreenAxisPickSlopPixels = 18.0f;
constexpr float kScreenPlanePickSlopPixels = 14.0f;
constexpr float kScreenCenterPickSlopPixels = 16.0f;
constexpr float kScaleAxisDragSensitivity = 0.2f;
constexpr float kMinUniformScreenRadius = 6.0f;

using namespace DirectX;

bool TryProjectWorldToScreen(
	const FEditorViewportMatrices& InMatrices,
	const FVector3& InWorldPosition,
	UINT InViewportWidth,
	UINT InViewportHeight,
	float InNearPlane,
	float InFarPlane,
	float* OutScreenX,
	float* OutScreenY,
	bool bRelaxedDepthClip = false)
{
	if (OutScreenX == nullptr || OutScreenY == nullptr || InViewportWidth == 0 || InViewportHeight == 0)
	{
		return false;
	}

	const XMVECTOR Projected = XMVector3Project(
		XMVectorSet(InWorldPosition.X, InWorldPosition.Y, InWorldPosition.Z, 1.0f),
		0.0f,
		0.0f,
		static_cast<float>(InViewportWidth),
		static_cast<float>(InViewportHeight),
		InNearPlane,
		InFarPlane,
		InMatrices.Projection,
		InMatrices.View,
		XMMatrixIdentity());
	const float ScreenX = XMVectorGetX(Projected);
	const float ScreenY = XMVectorGetY(Projected);
	const float ScreenZ = XMVectorGetZ(Projected);
	if (!std::isfinite(ScreenX) || !std::isfinite(ScreenY) || !std::isfinite(ScreenZ))
	{
		return false;
	}
	if (!bRelaxedDepthClip && (ScreenZ < 0.0f || ScreenZ > 1.0f))
	{
		return false;
	}
	if (bRelaxedDepthClip && ScreenZ < -0.25f)
	{
		return false;
	}

	*OutScreenX = ScreenX;
	*OutScreenY = ScreenY;
	return true;
}

float DistancePointToSegment2D(
	float InPointX,
	float InPointY,
	float InStartX,
	float InStartY,
	float InEndX,
	float InEndY)
{
	const float AbX = InEndX - InStartX;
	const float AbY = InEndY - InStartY;
	const float ApX = InPointX - InStartX;
	const float ApY = InPointY - InStartY;
	const float AbLenSq = AbX * AbX + AbY * AbY;
	if (AbLenSq < 1e-6f)
	{
		return std::hypot(ApX, ApY);
	}
	const float T = std::clamp((ApX * AbX + ApY * AbY) / AbLenSq, 0.0f, 1.0f);
	const float ClosestX = InStartX + AbX * T;
	const float ClosestY = InStartY + AbY * T;
	return std::hypot(InPointX - ClosestX, InPointY - ClosestY);
}

float MinScreenDistanceToAxisPolyline(
	const FEditorViewportMatrices& InMatrices,
	UINT InViewportWidth,
	UINT InViewportHeight,
	float InNearPlane,
	float InFarPlane,
	const FVector3& InOrigin,
	const FVector3& InAxisDirection,
	float InAxisLength,
	float InPickScreenX,
	float InPickScreenY)
{
	const float SampleTs[] = {0.0f, 0.35f, 0.65f, 0.85f, 1.0f};
	float MinDistance = std::numeric_limits<float>::max();
	float PrevScreenX = 0.0f;
	float PrevScreenY = 0.0f;
	bool bHasPrev = false;
	for (float SampleT : SampleTs)
	{
		const FVector3 WorldPoint = InOrigin + InAxisDirection * (InAxisLength * SampleT);
		float ScreenX = 0.0f;
		float ScreenY = 0.0f;
		if (!TryProjectWorldToScreen(
			InMatrices,
			WorldPoint,
			InViewportWidth,
			InViewportHeight,
			InNearPlane,
			InFarPlane,
			&ScreenX,
			&ScreenY,
			true))
		{
			bHasPrev = false;
			continue;
		}
		if (bHasPrev)
		{
			MinDistance = std::min(
				MinDistance,
				DistancePointToSegment2D(
					InPickScreenX, InPickScreenY, PrevScreenX, PrevScreenY, ScreenX, ScreenY));
		}
		PrevScreenX = ScreenX;
		PrevScreenY = ScreenY;
		bHasPrev = true;
	}
	return MinDistance;
}

bool RayClosestPointOnAxis(
	const FWorldRay& InRay,
	const FVector3& InAxisOrigin,
	const FVector3& InAxisDirection,
	FVector3* OutPointOnAxis)
{
	if (OutPointOnAxis == nullptr)
	{
		return false;
	}

	const FVector3 AxisDir = InAxisDirection.Normalized();
	const FVector3 RayToOrigin = InAxisOrigin - InRay.Origin;
	const FVector3 CrossDir = InRay.Direction.Cross(AxisDir);
	const float Denom = CrossDir.Dot(CrossDir);
	if (Denom < 1e-8f)
	{
		*OutPointOnAxis = InAxisOrigin;
		return true;
	}

	const float AxisT = RayToOrigin.Cross(InRay.Direction).Dot(CrossDir) / Denom;
	*OutPointOnAxis = InAxisOrigin + AxisDir * AxisT;
	return true;
}

bool IsPointInsideScreenRect(
	float InPointX,
	float InPointY,
	float InMinX,
	float InMinY,
	float InMaxX,
	float InMaxY,
	float InSlopPixels)
{
	return InPointX >= (InMinX - InSlopPixels) && InPointX <= (InMaxX + InSlopPixels) &&
		InPointY >= (InMinY - InSlopPixels) && InPointY <= (InMaxY + InSlopPixels);
}

DirectX::XMFLOAT3 ToXmFloat3(const FVector3& InVector)
{
	return DirectX::XMFLOAT3(InVector.X, InVector.Y, InVector.Z);
}

DirectX::XMFLOAT4 AxisColor(EGizmoAxis InAxis, float InAlpha = 1.0f)
{
	switch (InAxis)
	{
	case EGizmoAxis::X:
		return DirectX::XMFLOAT4(1.0f, 0.2f, 0.2f, InAlpha);
	case EGizmoAxis::Y:
		return DirectX::XMFLOAT4(0.2f, 0.9f, 0.2f, InAlpha);
	case EGizmoAxis::Z:
		return DirectX::XMFLOAT4(0.2f, 0.4f, 1.0f, InAlpha);
	case EGizmoAxis::XY:
		return DirectX::XMFLOAT4(0.85f, 0.85f, 0.2f, InAlpha);
	case EGizmoAxis::YZ:
		return DirectX::XMFLOAT4(0.2f, 0.85f, 0.85f, InAlpha);
	case EGizmoAxis::XZ:
		return DirectX::XMFLOAT4(0.85f, 0.2f, 0.85f, InAlpha);
	default:
		return DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, InAlpha);
	}
}

void AppendTriangle(
	std::vector<Dx12Renderer::Vertex>* OutVertices,
	const FVector3& InA,
	const FVector3& InB,
	const FVector3& InC,
	const DirectX::XMFLOAT4& InColor)
{
	if (OutVertices == nullptr)
	{
		return;
	}
	const FVector3 Normal = (InB - InA).Cross(InC - InA).Normalized();
	const DirectX::XMFLOAT3 XmNormal = ToXmFloat3(Normal);
	OutVertices->push_back({ToXmFloat3(InA), XmNormal, InColor});
	OutVertices->push_back({ToXmFloat3(InB), XmNormal, InColor});
	OutVertices->push_back({ToXmFloat3(InC), XmNormal, InColor});
}

void AppendDoubleSidedTriangle(
	std::vector<Dx12Renderer::Vertex>* OutVertices,
	const FVector3& InA,
	const FVector3& InB,
	const FVector3& InC,
	const DirectX::XMFLOAT4& InColor)
{
	AppendTriangle(OutVertices, InA, InB, InC, InColor);
	AppendTriangle(OutVertices, InA, InC, InB, InColor);
}

void BuildOrthonormalBasis(const FVector3& InAxisDir, FVector3* OutPerpU, FVector3* OutPerpV)
{
	const FVector3 UpReference = (std::fabs(InAxisDir.Z) < 0.9f)
		? FVector3(0.0f, 0.0f, 1.0f)
		: FVector3(0.0f, 1.0f, 0.0f);
	*OutPerpU = InAxisDir.Cross(UpReference).Normalized();
	*OutPerpV = InAxisDir.Cross(*OutPerpU).Normalized();
}

void AppendCylinder(
	std::vector<Dx12Renderer::Vertex>* OutVertices,
	const FVector3& InStart,
	const FVector3& InEnd,
	float InRadius,
	const DirectX::XMFLOAT4& InColor)
{
	const FVector3 Axis = InEnd - InStart;
	const float AxisLength = Axis.Length();
	if (AxisLength < 1e-5f || OutVertices == nullptr)
	{
		return;
	}

	const FVector3 AxisDir = Axis * (1.0f / AxisLength);
	FVector3 PerpU{};
	FVector3 PerpV{};
	BuildOrthonormalBasis(AxisDir, &PerpU, &PerpV);

	for (int32_t Segment = 0; Segment < kCylinderSegments; ++Segment)
	{
		const float Angle0 = (2.0f * kPi * static_cast<float>(Segment)) / static_cast<float>(kCylinderSegments);
		const float Angle1 = (2.0f * kPi * static_cast<float>(Segment + 1)) / static_cast<float>(kCylinderSegments);
		const FVector3 Offset0 = (PerpU * std::cos(Angle0) + PerpV * std::sin(Angle0)) * InRadius;
		const FVector3 Offset1 = (PerpU * std::cos(Angle1) + PerpV * std::sin(Angle1)) * InRadius;

		const FVector3 A0 = InStart + Offset0;
		const FVector3 A1 = InStart + Offset1;
		const FVector3 B0 = InEnd + Offset0;
		const FVector3 B1 = InEnd + Offset1;
		AppendTriangle(OutVertices, A0, B0, A1, InColor);
		AppendTriangle(OutVertices, A1, B0, B1, InColor);
	}
}

void AppendCone(
	std::vector<Dx12Renderer::Vertex>* OutVertices,
	const FVector3& InBaseCenter,
	const FVector3& InTip,
	float InBaseRadius,
	const DirectX::XMFLOAT4& InColor)
{
	const FVector3 Axis = InTip - InBaseCenter;
	const float AxisLength = Axis.Length();
	if (AxisLength < 1e-5f || OutVertices == nullptr)
	{
		return;
	}

	const FVector3 AxisDir = Axis * (1.0f / AxisLength);
	FVector3 PerpU{};
	FVector3 PerpV{};
	BuildOrthonormalBasis(AxisDir, &PerpU, &PerpV);

	for (int32_t Segment = 0; Segment < kConeSegments; ++Segment)
	{
		const float Angle0 = (2.0f * kPi * static_cast<float>(Segment)) / static_cast<float>(kConeSegments);
		const float Angle1 = (2.0f * kPi * static_cast<float>(Segment + 1)) / static_cast<float>(kConeSegments);
		const FVector3 Base0 = InBaseCenter + (PerpU * std::cos(Angle0) + PerpV * std::sin(Angle0)) * InBaseRadius;
		const FVector3 Base1 = InBaseCenter + (PerpU * std::cos(Angle1) + PerpV * std::sin(Angle1)) * InBaseRadius;
		AppendTriangle(OutVertices, InTip, Base0, Base1, InColor);
	}
}

void AppendAxisArrow(
	std::vector<Dx12Renderer::Vertex>* OutVertices,
	const FVector3& InOrigin,
	const FVector3& InAxisDir,
	float InScale,
	const DirectX::XMFLOAT4& InColor)
{
	const float ShaftLength = InScale * (1.0f - kArrowLengthFactor);
	const float Radius = InScale * kAxisCylinderRadiusFactor;
	const FVector3 ShaftEnd = InOrigin + InAxisDir * ShaftLength;
	const FVector3 AxisEnd = InOrigin + InAxisDir * InScale;
	AppendCylinder(OutVertices, InOrigin, ShaftEnd, Radius, InColor);
	AppendCone(OutVertices, ShaftEnd, AxisEnd, Radius * kArrowHeadRadiusFactor, InColor);
}

void GetRotateGizmoOctant(
	const FVector3& InGizmoOrigin,
	const FRotator3& InGizmoRotation,
	const FVector3& InCameraPosition,
	int32_t* OutSignX,
	int32_t* OutSignY,
	int32_t* OutSignZ)
{
	if (OutSignX == nullptr || OutSignY == nullptr || OutSignZ == nullptr)
	{
		return;
	}

	const FVector3 ToCamera = InCameraPosition - InGizmoOrigin;
	const FVector3 LocalAxisX = RotateGizmoDirection(InGizmoRotation, FVector3{1.0f, 0.0f, 0.0f});
	const FVector3 LocalAxisY = RotateGizmoDirection(InGizmoRotation, FVector3{0.0f, 1.0f, 0.0f});
	const FVector3 LocalAxisZ = RotateGizmoDirection(InGizmoRotation, FVector3{0.0f, 0.0f, 1.0f});
	*OutSignX = ToCamera.Dot(LocalAxisX) >= 0.0f ? 1 : -1;
	*OutSignY = ToCamera.Dot(LocalAxisY) >= 0.0f ? 1 : -1;
	*OutSignZ = ToCamera.Dot(LocalAxisZ) >= 0.0f ? 1 : -1;
}

void GetRotateRingArcAnglesForOctant(
	EGizmoAxis InAxis,
	int32_t InSignX,
	int32_t InSignY,
	int32_t InSignZ,
	float* OutStartAngle,
	float* OutEndAngle)
{
	if (OutStartAngle == nullptr || OutEndAngle == nullptr)
	{
		return;
	}

	int32_t SignU = 1;
	int32_t SignV = 1;
	switch (InAxis)
	{
	case EGizmoAxis::X:
		SignU = InSignY;
		SignV = InSignZ;
		break;
	case EGizmoAxis::Y:
		SignU = InSignX;
		SignV = InSignZ;
		break;
	case EGizmoAxis::Z:
	default:
		SignU = InSignX;
		SignV = InSignY;
		break;
	}

	*OutStartAngle = SignU > 0 ? 0.0f : kPi;
	*OutEndAngle = *OutStartAngle + static_cast<float>(SignU * SignV) * kHalfPi;
}

bool IsAngleWithinArc(float InAngle, float InStartAngle, float InEndAngle)
{
	const float Epsilon = 1e-4f;
	const float Span = InEndAngle - InStartAngle;
	float Delta = InAngle - InStartAngle;
	const float TwoPi = 2.0f * kPi;
	if (Span > 0.0f)
	{
		while (Delta < 0.0f)
		{
			Delta += TwoPi;
		}
		while (Delta >= TwoPi)
		{
			Delta -= TwoPi;
		}
		return Delta >= -Epsilon && Delta <= Span + Epsilon;
	}

	while (Delta > 0.0f)
	{
		Delta -= TwoPi;
	}
	while (Delta <= -TwoPi)
	{
		Delta += TwoPi;
	}
	return Delta <= Epsilon && Delta >= Span - Epsilon;
}

FVector3 BuildRingPointFromTangentAngles(
	const FVector3& InOrigin,
	const FVector3& InTangentU,
	const FVector3& InTangentV,
	float InAngleRadians,
	float InRadius)
{
	return InOrigin + (InTangentU * std::cos(InAngleRadians) + InTangentV * std::sin(InAngleRadians)) * InRadius;
}

float ComputeSignedRotateDragDeltaRadians(EGizmoAxis InAxis, float InDeltaRadians)
{
	if (InAxis == EGizmoAxis::Y)
	{
		return -InDeltaRadians;
	}
	return InDeltaRadians;
}

float NormalizeAngleDeltaShortest(float InDeltaRadians)
{
	const float TwoPi = 2.0f * kPi;
	float Result = InDeltaRadians;
	while (Result > kPi)
	{
		Result -= TwoPi;
	}
	while (Result < -kPi)
	{
		Result += TwoPi;
	}
	return Result;
}

void AppendRotateDragSector(
	std::vector<Dx12Renderer::Vertex>* OutVertices,
	const FVector3& InOrigin,
	const FVector3& InTangentU,
	const FVector3& InTangentV,
	float InStartAngleRadians,
	float InDeltaAngleRadians,
	float InRadius,
	const DirectX::XMFLOAT4& InColor)
{
	if (OutVertices == nullptr || std::fabs(InDeltaAngleRadians) < 1e-5f)
	{
		return;
	}

	const int32_t SectorSegments = std::max(
		4,
		static_cast<int32_t>(std::ceil(
			std::fabs(InDeltaAngleRadians) / kHalfPi * static_cast<float>(kRotateQuarterRingSegments))));
	for (int32_t Segment = 0; Segment < SectorSegments; ++Segment)
	{
		const float T0 = static_cast<float>(Segment) / static_cast<float>(SectorSegments);
		const float T1 = static_cast<float>(Segment + 1) / static_cast<float>(SectorSegments);
		const float Angle0 = InStartAngleRadians + InDeltaAngleRadians * T0;
		const float Angle1 = InStartAngleRadians + InDeltaAngleRadians * T1;
		const FVector3 Point0 = BuildRingPointFromTangentAngles(
			InOrigin, InTangentU, InTangentV, Angle0, InRadius);
		const FVector3 Point1 = BuildRingPointFromTangentAngles(
			InOrigin, InTangentU, InTangentV, Angle1, InRadius);
		AppendDoubleSidedTriangle(OutVertices, InOrigin, Point0, Point1, InColor);
	}
}

void AppendRotateDragTick(
	std::vector<Dx12Renderer::Vertex>* OutVertices,
	const FVector3& InOrigin,
	const FVector3& InTangentU,
	const FVector3& InTangentV,
	float InAngleRadians,
	float InRadius,
	float InTickExtent,
	float InTickRadius,
	const DirectX::XMFLOAT4& InColor)
{
	const FVector3 Point = BuildRingPointFromTangentAngles(
		InOrigin, InTangentU, InTangentV, InAngleRadians, InRadius);
	FVector3 Radial = Point - InOrigin;
	const float RadialLen = Radial.Length();
	if (RadialLen < 1e-5f)
	{
		return;
	}
	Radial = Radial * (1.0f / RadialLen);
	AppendCylinder(
		OutVertices,
		Point - Radial * InTickExtent,
		Point + Radial * InTickExtent,
		InTickRadius,
		InColor);
}

void AppendRotateDragPointer(
	std::vector<Dx12Renderer::Vertex>* OutVertices,
	const FVector3& InOrigin,
	const FVector3& InTangentU,
	const FVector3& InTangentV,
	const FVector3& InAxisDirection,
	float InAngleRadians,
	float InRadius,
	float InPointerSize,
	const DirectX::XMFLOAT4& InColor)
{
	const FVector3 RingPoint = BuildRingPointFromTangentAngles(
		InOrigin, InTangentU, InTangentV, InAngleRadians, InRadius);
	FVector3 RadialIn = InOrigin - RingPoint;
	const float RadialLen = RadialIn.Length();
	if (RadialLen < 1e-5f)
	{
		return;
	}
	RadialIn = RadialIn * (1.0f / RadialLen);
	const FVector3 Tangent = InAxisDirection.Cross(RingPoint - InOrigin).Normalized();
	const FVector3 Tip = RingPoint + RadialIn * (InPointerSize * 0.55f);
	const FVector3 BaseLeft = RingPoint + Tangent * (InPointerSize * 0.42f);
	const FVector3 BaseRight = RingPoint - Tangent * (InPointerSize * 0.42f);
	AppendTriangle(OutVertices, Tip, BaseLeft, BaseRight, InColor);
}

void AppendRotateDragVisuals(
	std::vector<Dx12Renderer::Vertex>* OutVertices,
	const FGizmoDragState& InDragState,
	float InScale,
	EGizmoAxis InAxis)
{
	if (OutVertices == nullptr || std::fabs(InDragState.AccumulatedDragAngleRadians) < 1e-5f)
	{
		return;
	}

	const DirectX::XMFLOAT4 SectorColor{0.95f, 0.82f, 0.08f, kRotateDragSectorColorAlpha};
	const float TickExtent = InScale * kRotateDragTickExtentFactor;
	const float TickRadius = InScale * kRotateDragTickRadiusFactor;
	const float PointerSize = InScale * kRotateDragPointerSizeFactor;
	const float CurrentAngle =
		InDragState.DragStartAngleRadians + InDragState.AccumulatedDragAngleRadians;

	AppendRotateDragSector(
		OutVertices,
		InDragState.GizmoOrigin,
		InDragState.RingTangentU,
		InDragState.RingTangentV,
		InDragState.DragStartAngleRadians,
		InDragState.AccumulatedDragAngleRadians,
		InScale,
		SectorColor);
	AppendRotateDragTick(
		OutVertices,
		InDragState.GizmoOrigin,
		InDragState.RingTangentU,
		InDragState.RingTangentV,
		InDragState.DragStartAngleRadians,
		InScale,
		TickExtent,
		TickRadius,
		DirectX::XMFLOAT4{1.0f, 1.0f, 1.0f, 1.0f});
	AppendRotateDragTick(
		OutVertices,
		InDragState.GizmoOrigin,
		InDragState.RingTangentU,
		InDragState.RingTangentV,
		CurrentAngle,
		InScale,
		TickExtent,
		TickRadius,
		AxisColor(InAxis, 1.0f));
	AppendRotateDragPointer(
		OutVertices,
		InDragState.GizmoOrigin,
		InDragState.RingTangentU,
		InDragState.RingTangentV,
		InDragState.AxisDirection,
		CurrentAngle,
		InScale,
		PointerSize,
		DirectX::XMFLOAT4{0.98f, 0.86f, 0.12f, 1.0f});
}

bool RayHitSlabAabb(
	const FVector3& InRayOrigin,
	const FVector3& InRayDirection,
	const FVector3& InBoxMin,
	const FVector3& InBoxMax,
	float* OutRayT)
{
	float TMin = 0.0f;
	float TMax = std::numeric_limits<float>::max();
	for (int32_t Axis = 0; Axis < 3; ++Axis)
	{
		const float Origin = (&InRayOrigin.X)[Axis];
		const float Direction = (&InRayDirection.X)[Axis];
		const float MinBound = (&InBoxMin.X)[Axis];
		const float MaxBound = (&InBoxMax.X)[Axis];
		if (std::fabs(Direction) < 1e-8f)
		{
			if (Origin < MinBound || Origin > MaxBound)
			{
				return false;
			}
			continue;
		}

		const float InvDirection = 1.0f / Direction;
		float T0 = (MinBound - Origin) * InvDirection;
		float T1 = (MaxBound - Origin) * InvDirection;
		if (T0 > T1)
		{
			std::swap(T0, T1);
		}
		TMin = std::max(TMin, T0);
		TMax = std::min(TMax, T1);
		if (TMax < TMin)
		{
			return false;
		}
	}

	if (TMax < 0.0f)
	{
		return false;
	}

	if (OutRayT != nullptr)
	{
		*OutRayT = TMin >= 0.0f ? TMin : TMax;
	}
	return true;
}

bool RayHitOrientedBox(
	const FWorldRay& InRay,
	const FVector3& InCenter,
	float InHalfExtent,
	const FVector3& InAxisX,
	const FVector3& InAxisY,
	const FVector3& InAxisZ,
	float* OutRayT)
{
	const FVector3 AxisX = InAxisX.Normalized();
	const FVector3 AxisY = InAxisY.Normalized();
	const FVector3 AxisZ = InAxisZ.Normalized();
	const FVector3 RelativeOrigin = InRay.Origin - InCenter;
	const FVector3 LocalOrigin(
		RelativeOrigin.Dot(AxisX),
		RelativeOrigin.Dot(AxisY),
		RelativeOrigin.Dot(AxisZ));
	const FVector3 LocalDirection(
		InRay.Direction.Dot(AxisX),
		InRay.Direction.Dot(AxisY),
		InRay.Direction.Dot(AxisZ));
	const FVector3 BoxMin(-InHalfExtent, -InHalfExtent, -InHalfExtent);
	const FVector3 BoxMax(InHalfExtent, InHalfExtent, InHalfExtent);
	return RayHitSlabAabb(LocalOrigin, LocalDirection, BoxMin, BoxMax, OutRayT);
}

void AppendPlaneQuad(
	std::vector<Dx12Renderer::Vertex>* OutVertices,
	const FVector3& InOrigin,
	const FVector3& InAxisU,
	const FVector3& InAxisV,
	float InPlaneSize,
	const DirectX::XMFLOAT4& InColor)
{
	const FVector3 Corner0 = InOrigin;
	const FVector3 Corner1 = InOrigin + InAxisU * InPlaneSize;
	const FVector3 Corner2 = InOrigin + InAxisU * InPlaneSize + InAxisV * InPlaneSize;
	const FVector3 Corner3 = InOrigin + InAxisV * InPlaneSize;
	AppendTriangle(OutVertices, Corner0, Corner1, Corner2, InColor);
	AppendTriangle(OutVertices, Corner0, Corner2, Corner3, InColor);
}

void AppendOrientedCenterBox(
	std::vector<Dx12Renderer::Vertex>* OutVertices,
	const FVector3& InOrigin,
	float InHalfExtent,
	const FVector3& InAxisX,
	const FVector3& InAxisY,
	const FVector3& InAxisZ,
	const DirectX::XMFLOAT4& InColor)
{
	if (OutVertices == nullptr)
	{
		return;
	}

	const FVector3 HX = InAxisX * InHalfExtent;
	const FVector3 HY = InAxisY * InHalfExtent;
	const FVector3 HZ = InAxisZ * InHalfExtent;
	const FVector3 Corners[8] = {
		InOrigin - HX - HY - HZ,
		InOrigin + HX - HY - HZ,
		InOrigin + HX + HY - HZ,
		InOrigin - HX + HY - HZ,
		InOrigin - HX - HY + HZ,
		InOrigin + HX - HY + HZ,
		InOrigin + HX + HY + HZ,
		InOrigin - HX + HY + HZ,
	};
	const int Faces[12][3] = {
		{0, 1, 2}, {0, 2, 3},
		{4, 6, 5}, {4, 7, 6},
		{0, 4, 5}, {0, 5, 1},
		{2, 6, 7}, {2, 7, 3},
		{0, 3, 7}, {0, 7, 4},
		{1, 5, 6}, {1, 6, 2},
	};
	for (const auto& Face : Faces)
	{
		AppendTriangle(OutVertices, Corners[Face[0]], Corners[Face[1]], Corners[Face[2]], InColor);
	}
}

void AppendAxisScaleHandle(
	std::vector<Dx12Renderer::Vertex>* OutVertices,
	const FVector3& InOrigin,
	const FVector3& InAxisDir,
	const FVector3& InGizmoAxisX,
	const FVector3& InGizmoAxisY,
	const FVector3& InGizmoAxisZ,
	float InScale,
	const DirectX::XMFLOAT4& InColor)
{
	const float HandleHalf = InScale * kScaleHandleHalfExtentFactor;
	const float Radius = InScale * kAxisCylinderRadiusFactor;
	const FVector3 HandleCenter = InOrigin + InAxisDir * InScale;
	const FVector3 ShaftEnd = HandleCenter - InAxisDir * (HandleHalf * 2.0f);
	AppendCylinder(OutVertices, InOrigin, ShaftEnd, Radius, InColor);
	AppendOrientedCenterBox(
		OutVertices,
		HandleCenter,
		HandleHalf,
		InGizmoAxisX,
		InGizmoAxisY,
		InGizmoAxisZ,
		InColor);
}

void AppendScaleConnectorLines(
	std::vector<Dx12Renderer::Vertex>* OutVertices,
	const FVector3& InEndX,
	const FVector3& InEndY,
	const FVector3& InEndZ,
	float InConnectorRadius,
	const DirectX::XMFLOAT4& InColorXY,
	const DirectX::XMFLOAT4& InColorXZ,
	const DirectX::XMFLOAT4& InColorYZ)
{
	AppendCylinder(OutVertices, InEndX, InEndY, InConnectorRadius, InColorXY);
	AppendCylinder(OutVertices, InEndX, InEndZ, InConnectorRadius, InColorXZ);
	AppendCylinder(OutVertices, InEndY, InEndZ, InConnectorRadius, InColorYZ);
}

void AppendCenterBox(
	std::vector<Dx12Renderer::Vertex>* OutVertices,
	const FVector3& InOrigin,
	float InHalfExtent,
	const DirectX::XMFLOAT4& InColor)
{
	const FVector3 MinCorner = InOrigin - FVector3(InHalfExtent, InHalfExtent, InHalfExtent);
	const FVector3 MaxCorner = InOrigin + FVector3(InHalfExtent, InHalfExtent, InHalfExtent);
	const FVector3 Corners[8] = {
		{MinCorner.X, MinCorner.Y, MinCorner.Z},
		{MaxCorner.X, MinCorner.Y, MinCorner.Z},
		{MaxCorner.X, MaxCorner.Y, MinCorner.Z},
		{MinCorner.X, MaxCorner.Y, MinCorner.Z},
		{MinCorner.X, MinCorner.Y, MaxCorner.Z},
		{MaxCorner.X, MinCorner.Y, MaxCorner.Z},
		{MaxCorner.X, MaxCorner.Y, MaxCorner.Z},
		{MinCorner.X, MaxCorner.Y, MaxCorner.Z},
	};
	const int Faces[12][3] = {
		{0, 1, 2}, {0, 2, 3},
		{4, 6, 5}, {4, 7, 6},
		{0, 4, 5}, {0, 5, 1},
		{2, 6, 7}, {2, 7, 3},
		{0, 3, 7}, {0, 7, 4},
		{1, 5, 6}, {1, 6, 2},
	};
	for (const auto& Face : Faces)
	{
		AppendTriangle(OutVertices, Corners[Face[0]], Corners[Face[1]], Corners[Face[2]], InColor);
	}
}

bool RayPlaneIntersect(
	const FWorldRay& InRay,
	const FVector3& InPlanePoint,
	const FVector3& InPlaneNormal,
	FVector3* OutHit)
{
	const float Denom = InRay.Direction.Dot(InPlaneNormal);
	if (std::fabs(Denom) < 1e-5f)
	{
		return false;
	}
	const float T = (InPlanePoint - InRay.Origin).Dot(InPlaneNormal) / Denom;
	if (T < 0.0f)
	{
		return false;
	}
	if (OutHit != nullptr)
	{
		*OutHit = InRay.Origin + InRay.Direction * T;
	}
	return true;
}

bool ComputeLineAxisDragHitPoint(
	const FWorldRay& InRay,
	const FVector3& InGizmoOrigin,
	const FVector3& InAxisDirection,
	const Dx12Renderer::CameraState& InCamera,
	FVector3* OutHitPoint)
{
	if (OutHitPoint == nullptr)
	{
		return false;
	}

	FVector3 PlaneNormal = InAxisDirection.Cross(
		FVector3(InCamera.position.x, InCamera.position.y, InCamera.position.z) - InGizmoOrigin);
	if (PlaneNormal.Length() < 1e-3f)
	{
		PlaneNormal = FVector3(0.0f, 0.0f, 1.0f);
	}
	PlaneNormal = PlaneNormal.Normalized();
	if (RayPlaneIntersect(InRay, InGizmoOrigin, PlaneNormal, OutHitPoint))
	{
		return true;
	}
	return RayClosestPointOnAxis(InRay, InGizmoOrigin, InAxisDirection, OutHitPoint);
}

bool TrySetupScreenLineDrag(
	const FEditorViewportMatrices& InMatrices,
	const FVector3& InGizmoOrigin,
	const FVector3& InAxisDirection,
	float InGizmoScale,
	UINT InViewportWidth,
	UINT InViewportHeight,
	float InNearPlane,
	float InFarPlane,
	float InScreenX,
	float InScreenY,
	FGizmoDragState* OutDragState)
{
	if (OutDragState == nullptr || InGizmoScale < 1e-6f)
	{
		return false;
	}

	const FVector3 AxisEnd = InGizmoOrigin + InAxisDirection * InGizmoScale;
	float OriginScreenX = 0.0f;
	float OriginScreenY = 0.0f;
	float AxisEndScreenX = 0.0f;
	float AxisEndScreenY = 0.0f;
	if (!TryProjectWorldToScreen(
		InMatrices,
		InGizmoOrigin,
		InViewportWidth,
		InViewportHeight,
		InNearPlane,
		InFarPlane,
		&OriginScreenX,
		&OriginScreenY,
		true) ||
		!TryProjectWorldToScreen(
			InMatrices,
			AxisEnd,
			InViewportWidth,
			InViewportHeight,
			InNearPlane,
			InFarPlane,
			&AxisEndScreenX,
			&AxisEndScreenY,
			true))
	{
		return false;
	}

	const float ScreenAxisX = AxisEndScreenX - OriginScreenX;
	const float ScreenAxisY = AxisEndScreenY - OriginScreenY;
	const float ScreenAxisLen = std::hypot(ScreenAxisX, ScreenAxisY);
	if (ScreenAxisLen < 2.0f)
	{
		return false;
	}

	OutDragState->bScreenLineDragActive = true;
	OutDragState->DragStartScreenX = InScreenX;
	OutDragState->DragStartScreenY = InScreenY;
	OutDragState->ScreenAxisDirX = ScreenAxisX / ScreenAxisLen;
	OutDragState->ScreenAxisDirY = ScreenAxisY / ScreenAxisLen;
	OutDragState->WorldUnitsPerScreenPixelAlongAxis = InGizmoScale / ScreenAxisLen;
	return true;
}

FVector3 ProjectPointOnAxis(const FVector3& InPoint, const FVector3& InAxisOrigin, const FVector3& InAxisDir)
{
	const FVector3 Delta = InPoint - InAxisOrigin;
	const float Param = Delta.Dot(InAxisDir);
	return InAxisOrigin + InAxisDir * Param;
}
} // namespace

EGizmoMode FEditorTransformGizmo::GetMode() const
{
	return GizmoMode_;
}

void FEditorTransformGizmo::SetMode(EGizmoMode InMode)
{
	GizmoMode_ = InMode;
}

bool FEditorTransformGizmo::IsDragging() const
{
	return DragState_.bIsDragging;
}

void FEditorTransformGizmo::SetHoveredAxis(EGizmoAxis InAxis)
{
	HoveredAxis_ = InAxis;
}

EGizmoAxis FEditorTransformGizmo::GetHoveredAxis() const
{
	return HoveredAxis_;
}

FVector3 FEditorTransformGizmo::GetGizmoOrigin(const AActor* InActor) const
{
	if (InActor == nullptr)
	{
		return FVector3();
	}
	if (USceneComponent* Root = InActor->GetRootComponent())
	{
		return Root->GetWorldLocation();
	}
	return InActor->GetActorTransform().Position;
}

FRotator3 FEditorTransformGizmo::GetGizmoRotation(const AActor* InActor) const
{
	if (InActor == nullptr)
	{
		return FRotator3{};
	}
	return InActor->GetActorTransform().Rotation;
}

float FEditorTransformGizmo::ComputeGizmoScale(
	const FVector3& InGizmoOrigin,
	const Dx12Renderer::CameraState& InCamera) const
{
	const FVector3 CameraPos(InCamera.position.x, InCamera.position.y, InCamera.position.z);
	const float Distance = std::max((InGizmoOrigin - CameraPos).Length(), 0.5f);
	return Distance * kGizmoScreenScaleFactor;
}

FVector3 FEditorTransformGizmo::GetLocalAxisDirection(EGizmoAxis InAxis) const
{
	switch (InAxis)
	{
	case EGizmoAxis::X:
		return FVector3(1.0f, 0.0f, 0.0f);
	case EGizmoAxis::Y:
		return FVector3(0.0f, 1.0f, 0.0f);
	case EGizmoAxis::Z:
		return FVector3(0.0f, 0.0f, 1.0f);
	default:
		return FVector3(1.0f, 0.0f, 0.0f);
	}
}

FVector3 FEditorTransformGizmo::GetWorldAxisDirection(EGizmoAxis InAxis, const FRotator3& InGizmoRotation) const
{
	return RotateGizmoDirection(InGizmoRotation, GetLocalAxisDirection(InAxis)).Normalized();
}

bool FEditorTransformGizmo::IsPlaneAxis(EGizmoAxis InAxis) const
{
	return InAxis == EGizmoAxis::XY || InAxis == EGizmoAxis::YZ || InAxis == EGizmoAxis::XZ;
}

bool FEditorTransformGizmo::IsLineAxis(EGizmoAxis InAxis) const
{
	return InAxis == EGizmoAxis::X || InAxis == EGizmoAxis::Y || InAxis == EGizmoAxis::Z;
}

void FEditorTransformGizmo::GetWorldPlaneAxes(
	EGizmoAxis InPlaneAxis,
	const FRotator3& InGizmoRotation,
	FVector3* OutAxisU,
	FVector3* OutAxisV,
	FVector3* OutPlaneNormal) const
{
	if (OutAxisU == nullptr || OutAxisV == nullptr || OutPlaneNormal == nullptr)
	{
		return;
	}

	FVector3 LocalU{};
	FVector3 LocalV{};
	FVector3 LocalNormal{};
	switch (InPlaneAxis)
	{
	case EGizmoAxis::XY:
		LocalU = FVector3(1.0f, 0.0f, 0.0f);
		LocalV = FVector3(0.0f, 1.0f, 0.0f);
		LocalNormal = FVector3(0.0f, 0.0f, 1.0f);
		break;
	case EGizmoAxis::YZ:
		LocalU = FVector3(0.0f, 1.0f, 0.0f);
		LocalV = FVector3(0.0f, 0.0f, 1.0f);
		LocalNormal = FVector3(1.0f, 0.0f, 0.0f);
		break;
	case EGizmoAxis::XZ:
		LocalU = FVector3(1.0f, 0.0f, 0.0f);
		LocalV = FVector3(0.0f, 0.0f, 1.0f);
		LocalNormal = FVector3(0.0f, 1.0f, 0.0f);
		break;
	default:
		LocalU = FVector3(1.0f, 0.0f, 0.0f);
		LocalV = FVector3(0.0f, 1.0f, 0.0f);
		LocalNormal = FVector3(0.0f, 0.0f, 1.0f);
		break;
	}

	*OutAxisU = RotateGizmoDirection(InGizmoRotation, LocalU).Normalized();
	*OutAxisV = RotateGizmoDirection(InGizmoRotation, LocalV).Normalized();
	*OutPlaneNormal = RotateGizmoDirection(InGizmoRotation, LocalNormal).Normalized();
}

bool FEditorTransformGizmo::RayHitCylinder(
	const FWorldRay& InRay,
	const FVector3& InSegmentStart,
	const FVector3& InSegmentEnd,
	float InRadius,
	float* OutRayT) const
{
	const FVector3 Segment = InSegmentEnd - InSegmentStart;
	const float SegmentLengthSq = Segment.Dot(Segment);
	if (SegmentLengthSq < 1e-8f)
	{
		return false;
	}

	const FVector3 ToStart = InRay.Origin - InSegmentStart;
	const float SegmentDotDir = Segment.Dot(InRay.Direction);
	const float SegmentDotToStart = Segment.Dot(ToStart);
	const float DirDotToStart = InRay.Direction.Dot(ToStart);
	const float ToStartSq = ToStart.Dot(ToStart);

	const float Denom = SegmentLengthSq - SegmentDotDir * SegmentDotDir;
	float RayT = 0.0f;
	float AxisParam = 0.0f;
	if (std::fabs(Denom) < 1e-8f)
	{
		RayT = -DirDotToStart;
		AxisParam = SegmentDotToStart / SegmentLengthSq;
	}
	else
	{
		RayT = (SegmentDotDir * SegmentDotToStart - SegmentDotDir * DirDotToStart - SegmentLengthSq * DirDotToStart) / Denom;
		AxisParam = (SegmentDotDir * RayT + SegmentDotToStart) / SegmentLengthSq;
	}

	AxisParam = std::clamp(AxisParam, 0.0f, 1.0f);
	const FVector3 ClosestOnAxis = InSegmentStart + Segment * AxisParam;
	const FVector3 ClosestOnRay = InRay.Origin + InRay.Direction * RayT;
	const float DistSq = (ClosestOnAxis - ClosestOnRay).Length();
	const float RadiusSq = InRadius * InRadius;
	if (DistSq > RadiusSq || RayT < 0.0f)
	{
		return false;
	}

	if (OutRayT != nullptr)
	{
		*OutRayT = RayT;
	}
	return true;
}

bool FEditorTransformGizmo::RayHitPlaneQuad(
	const FWorldRay& InRay,
	const FVector3& InOrigin,
	const FVector3& InAxisU,
	const FVector3& InAxisV,
	float InPlaneSize,
	float* OutRayT) const
{
	const FVector3 PlaneNormal = InAxisU.Cross(InAxisV).Normalized();
	FVector3 HitPoint{};
	if (!RayPlaneIntersect(InRay, InOrigin, PlaneNormal, &HitPoint))
	{
		return false;
	}

	const FVector3 Local = HitPoint - InOrigin;
	const float U = Local.Dot(InAxisU);
	const float V = Local.Dot(InAxisV);
	const float Epsilon = InPlaneSize * 0.05f;
	if (U < -Epsilon || V < -Epsilon || U > InPlaneSize + Epsilon || V > InPlaneSize + Epsilon)
	{
		return false;
	}

	if (OutRayT != nullptr)
	{
		*OutRayT = (HitPoint - InRay.Origin).Dot(InRay.Direction);
	}
	return true;
}

bool FEditorTransformGizmo::RayHitCenterBox(
	const FWorldRay& InRay,
	const FVector3& InOrigin,
	float InHalfExtent,
	float* OutRayT) const
{
	const FVector3 BoxMin = InOrigin - FVector3(InHalfExtent, InHalfExtent, InHalfExtent);
	const FVector3 BoxMax = InOrigin + FVector3(InHalfExtent, InHalfExtent, InHalfExtent);

	float TMin = 0.0f;
	float TMax = std::numeric_limits<float>::max();
	const auto Slab = [&](float InOriginComponent, float InDirComponent, float InMin, float InMax) -> bool
	{
		if (std::fabs(InDirComponent) < 1e-5f)
		{
			return InOriginComponent >= InMin && InOriginComponent <= InMax;
		}
		float InvD = 1.0f / InDirComponent;
		float T0 = (InMin - InOriginComponent) * InvD;
		float T1 = (InMax - InOriginComponent) * InvD;
		if (T0 > T1)
		{
			std::swap(T0, T1);
		}
		TMin = std::max(TMin, T0);
		TMax = std::min(TMax, T1);
		return TMax >= TMin;
	};

	if (!Slab(InRay.Origin.X, InRay.Direction.X, BoxMin.X, BoxMax.X) ||
		!Slab(InRay.Origin.Y, InRay.Direction.Y, BoxMin.Y, BoxMax.Y) ||
		!Slab(InRay.Origin.Z, InRay.Direction.Z, BoxMin.Z, BoxMax.Z))
	{
		return false;
	}

	if (OutRayT != nullptr)
	{
		*OutRayT = (TMin >= 0.0f) ? TMin : TMax;
	}
	return TMax >= 0.0f;
}

EGizmoAxis FEditorTransformGizmo::HitTest(
	const AActor* InActor,
	const Dx12Renderer::CameraState& InCamera,
	UINT InViewportWidth,
	UINT InViewportHeight,
	float InNearPlane,
	float InFarPlane,
	float InScreenX,
	float InScreenY,
	float InPickToleranceScale) const
{
	if (InActor == nullptr)
	{
		return EGizmoAxis::None;
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

	const FVector3 Origin = GetGizmoOrigin(InActor);
	const FRotator3 GizmoRotation = GetGizmoRotation(InActor);
	const float Scale = ComputeGizmoScale(Origin, InCamera);
	const float PickScale = std::max(1.0f, InPickToleranceScale);
	const float CylinderRadius = Scale * kAxisCylinderRadiusFactor * 1.15f * PickScale;
	const float ConeRadius = CylinderRadius * kArrowHeadRadiusFactor;
	const float PlaneSize = Scale * kPlaneHandleSizeFactor * PickScale;
	const float CenterHalfExtent = Scale * kCenterBoxHalfExtentFactor * PickScale;
	const float ShaftLength = Scale * (1.0f - kArrowLengthFactor);

	struct FHitCandidate
	{
		EGizmoAxis Axis = EGizmoAxis::None;
		float RayT = std::numeric_limits<float>::max();
		int32_t Priority = 0;
	};

	auto ConsiderCandidate = [](FHitCandidate& InBest, const FHitCandidate& InCandidate)
	{
		if (InCandidate.Axis == EGizmoAxis::None)
		{
			return;
		}
		if (InCandidate.Priority > InBest.Priority ||
			(InCandidate.Priority == InBest.Priority && InCandidate.RayT < InBest.RayT))
		{
			InBest = InCandidate;
		}
	};

	FHitCandidate BestHit{};

	if (GizmoMode_ == EGizmoMode::Rotate)
	{
		const float TubeRadius = Scale * kRotateTubeRadiusFactor * PickScale;
		const FVector3 CameraPos(InCamera.position.x, InCamera.position.y, InCamera.position.z);
		int32_t OctantSignX = 1;
		int32_t OctantSignY = 1;
		int32_t OctantSignZ = 1;
		GetRotateGizmoOctant(Origin, GizmoRotation, CameraPos, &OctantSignX, &OctantSignY, &OctantSignZ);
		const EGizmoAxis Axes[] = {EGizmoAxis::X, EGizmoAxis::Y, EGizmoAxis::Z};
		for (EGizmoAxis Axis : Axes)
		{
			const FVector3 PlaneNormal = GetWorldAxisDirection(Axis, GizmoRotation);
			FVector3 HitPoint{};
			if (!RayPlaneIntersect(Ray, Origin, PlaneNormal, &HitPoint))
			{
				continue;
			}
			const FVector3 Offset = HitPoint - Origin;
			const FVector3 AxisDir = GetWorldAxisDirection(Axis, GizmoRotation);
			const FVector3 RadialOffset = Offset - AxisDir * Offset.Dot(AxisDir);
			const float DistToRing = std::fabs(RadialOffset.Length() - Scale);
			if (DistToRing > TubeRadius)
			{
				continue;
			}

			FVector3 LocalTangentU{};
			FVector3 LocalTangentV{};
			GetLocalRingTangentAxes(Axis, &LocalTangentU, &LocalTangentV);
			const FVector3 WorldTangentU = RotateGizmoDirection(GizmoRotation, LocalTangentU).Normalized();
			const FVector3 WorldTangentV = RotateGizmoDirection(GizmoRotation, LocalTangentV).Normalized();
			const float HitAngle = std::atan2(
				RadialOffset.Dot(WorldTangentV),
				RadialOffset.Dot(WorldTangentU));
			float ArcStartAngle = 0.0f;
			float ArcEndAngle = 0.0f;
			GetRotateRingArcAnglesForOctant(
				Axis, OctantSignX, OctantSignY, OctantSignZ, &ArcStartAngle, &ArcEndAngle);
			if (!IsAngleWithinArc(HitAngle, ArcStartAngle, ArcEndAngle))
			{
				continue;
			}

			const float RayT = (HitPoint - Ray.Origin).Dot(Ray.Direction);
			if (RayT < 0.0f)
			{
				continue;
			}
			ConsiderCandidate(BestHit, {Axis, RayT, 2});
		}
		return BestHit.Axis;
	}

	const FVector3 GizmoAxisX = GetWorldAxisDirection(EGizmoAxis::X, GizmoRotation);
	const FVector3 GizmoAxisY = GetWorldAxisDirection(EGizmoAxis::Y, GizmoRotation);
	const FVector3 GizmoAxisZ = GetWorldAxisDirection(EGizmoAxis::Z, GizmoRotation);

	const float ScreenAxisSlop = kScreenAxisPickSlopPixels * PickScale;
	const EGizmoAxis LineAxes[] = {EGizmoAxis::X, EGizmoAxis::Y, EGizmoAxis::Z};
	for (EGizmoAxis Axis : LineAxes)
	{
		const FVector3 AxisDir = (Axis == EGizmoAxis::X) ? GizmoAxisX : ((Axis == EGizmoAxis::Y) ? GizmoAxisY : GizmoAxisZ);
		const FVector3 ShaftEnd = Origin + AxisDir * ShaftLength;
		const FVector3 AxisEnd = Origin + AxisDir * Scale;
		float RayT = 0.0f;
		if (RayHitCylinder(Ray, Origin, ShaftEnd, CylinderRadius, &RayT))
		{
			ConsiderCandidate(BestHit, {Axis, RayT, 3});
		}
		else if (GizmoMode_ == EGizmoMode::Scale)
		{
			const float HandleHalf = Scale * kScaleHandleHalfExtentFactor * PickScale;
			const FVector3 HandleCenter = Origin + AxisDir * Scale;
			if (RayHitOrientedBox(Ray, HandleCenter, HandleHalf, GizmoAxisX, GizmoAxisY, GizmoAxisZ, &RayT))
			{
				ConsiderCandidate(BestHit, {Axis, RayT, 3});
			}
		}
		else if (RayHitCylinder(Ray, ShaftEnd, AxisEnd, ConeRadius, &RayT))
		{
			ConsiderCandidate(BestHit, {Axis, RayT, 3});
		}

		const float ScreenDistance = MinScreenDistanceToAxisPolyline(
			Matrices,
			InViewportWidth,
			InViewportHeight,
			InNearPlane,
			InFarPlane,
			Origin,
			AxisDir,
			Scale,
			InScreenX,
			InScreenY);
		if (ScreenDistance <= ScreenAxisSlop)
		{
			ConsiderCandidate(BestHit, {Axis, ScreenDistance, 4});
		}
	}

	const EGizmoAxis PlaneAxes[] = {EGizmoAxis::XY, EGizmoAxis::YZ, EGizmoAxis::XZ};
	if (GizmoMode_ != EGizmoMode::Scale)
	{
	for (EGizmoAxis PlaneAxis : PlaneAxes)
	{
		FVector3 AxisU{};
		FVector3 AxisV{};
		FVector3 PlaneNormal{};
		GetWorldPlaneAxes(PlaneAxis, GizmoRotation, &AxisU, &AxisV, &PlaneNormal);
		float RayT = 0.0f;
		if (RayHitPlaneQuad(Ray, Origin, AxisU, AxisV, PlaneSize, &RayT))
		{
			ConsiderCandidate(BestHit, {PlaneAxis, RayT, 2});
		}

		const FVector3 Corner0 = Origin;
		const FVector3 Corner1 = Origin + AxisU * PlaneSize;
		const FVector3 Corner2 = Origin + AxisU * PlaneSize + AxisV * PlaneSize;
		const FVector3 Corner3 = Origin + AxisV * PlaneSize;
		float MinX = 0.0f;
		float MinY = 0.0f;
		float MaxX = 0.0f;
		float MaxY = 0.0f;
		bool bHasScreenBounds = false;
		const FVector3 Corners[4] = {Corner0, Corner1, Corner2, Corner3};
		for (const FVector3& Corner : Corners)
		{
			float ScreenX = 0.0f;
			float ScreenY = 0.0f;
			if (!TryProjectWorldToScreen(
				Matrices, Corner, InViewportWidth, InViewportHeight, InNearPlane, InFarPlane, &ScreenX, &ScreenY))
			{
				continue;
			}
			if (!bHasScreenBounds)
			{
				MinX = MaxX = ScreenX;
				MinY = MaxY = ScreenY;
				bHasScreenBounds = true;
			}
			else
			{
				MinX = std::min(MinX, ScreenX);
				MinY = std::min(MinY, ScreenY);
				MaxX = std::max(MaxX, ScreenX);
				MaxY = std::max(MaxY, ScreenY);
			}
		}
		if (bHasScreenBounds &&
			IsPointInsideScreenRect(
				InScreenX, InScreenY, MinX, MinY, MaxX, MaxY, kScreenPlanePickSlopPixels * PickScale))
		{
			ConsiderCandidate(BestHit, {PlaneAxis, 0.0f, 4});
		}
	}
	}

	const int32_t UniformCenterPriority = (GizmoMode_ == EGizmoMode::Scale) ? 5 : 1;
	const int32_t UniformScreenPriority = (GizmoMode_ == EGizmoMode::Scale) ? 6 : 4;

	float CenterRayT = 0.0f;
	if (RayHitCenterBox(Ray, Origin, CenterHalfExtent, &CenterRayT))
	{
		ConsiderCandidate(BestHit, {EGizmoAxis::Uniform, CenterRayT, UniformCenterPriority});
	}

	float CenterScreenX = 0.0f;
	float CenterScreenY = 0.0f;
	if (TryProjectWorldToScreen(
		Matrices, Origin, InViewportWidth, InViewportHeight, InNearPlane, InFarPlane, &CenterScreenX, &CenterScreenY))
	{
		const float CenterDistance = std::hypot(InScreenX - CenterScreenX, InScreenY - CenterScreenY);
		const float CenterPickSlop = (GizmoMode_ == EGizmoMode::Scale)
			? kScreenCenterPickSlopPixels * 1.75f * PickScale
			: kScreenCenterPickSlopPixels * PickScale;
		if (CenterDistance <= CenterPickSlop)
		{
			ConsiderCandidate(BestHit, {EGizmoAxis::Uniform, CenterDistance, UniformScreenPriority});
		}
	}

	return BestHit.Axis;
}

bool FEditorTransformGizmo::IsScreenPointNearGizmo(
	const AActor* InActor,
	const Dx12Renderer::CameraState& InCamera,
	UINT InViewportWidth,
	UINT InViewportHeight,
	float InNearPlane,
	float InFarPlane,
	float InScreenX,
	float InScreenY,
	float InScreenSlopPixels) const
{
	return HitTest(
		InActor,
		InCamera,
		InViewportWidth,
		InViewportHeight,
		InNearPlane,
		InFarPlane,
		InScreenX,
		InScreenY,
		std::max(1.0f, InScreenSlopPixels / kScreenAxisPickSlopPixels)) != EGizmoAxis::None;
}

bool FEditorTransformGizmo::TryBeginDrag(
	const AActor* InActor,
	const Dx12Renderer::CameraState& InCamera,
	UINT InViewportWidth,
	UINT InViewportHeight,
	float InNearPlane,
	float InFarPlane,
	float InScreenX,
	float InScreenY,
	EGizmoAxis InPreferredAxis,
	float InPickToleranceScale)
{
	EGizmoAxis Axis = InPreferredAxis;
	if (Axis == EGizmoAxis::None)
	{
		Axis = HitTest(
			InActor,
			InCamera,
			InViewportWidth,
			InViewportHeight,
			InNearPlane,
			InFarPlane,
			InScreenX,
			InScreenY,
			InPickToleranceScale);
	}
	if (Axis == EGizmoAxis::None || InActor == nullptr)
	{
		return false;
	}

	DragState_ = FGizmoDragState{};
	DragState_.bIsDragging = true;
	DragState_.ActiveAxis = Axis;
	DragState_.DragStartTransform = InActor->GetActorTransform();
	DragState_.GizmoOrigin = GetGizmoOrigin(InActor);
	DragState_.GizmoRotation = GetGizmoRotation(InActor);
	DragState_.AxisDirection =
		IsLineAxis(Axis) ? GetWorldAxisDirection(Axis, DragState_.GizmoRotation) : FVector3();

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

	FVector3 HitPoint{};
	if (GizmoMode_ == EGizmoMode::Rotate)
	{
		const FVector3 PlaneNormal = GetWorldAxisDirection(Axis, DragState_.GizmoRotation);
		if (!RayPlaneIntersect(Ray, DragState_.GizmoOrigin, PlaneNormal, &HitPoint))
		{
			DragState_.bIsDragging = false;
			return false;
		}
		const FVector3 Offset = HitPoint - DragState_.GizmoOrigin;
		FVector3 LocalTangentU{};
		FVector3 LocalTangentV{};
		GetLocalRingTangentAxes(Axis, &LocalTangentU, &LocalTangentV);
		DragState_.RingTangentU = RotateGizmoDirection(DragState_.GizmoRotation, LocalTangentU).Normalized();
		DragState_.RingTangentV = RotateGizmoDirection(DragState_.GizmoRotation, LocalTangentV).Normalized();
		DragState_.AxisDirection = PlaneNormal;
		DragState_.DragStartAngleRadians =
			ComputeRotateRingAngleRadians(Offset, DragState_.RingTangentU, DragState_.RingTangentV);
		DragState_.CurrentDragAngleRadians = DragState_.DragStartAngleRadians;
		DragState_.PreviousDragAngleRadians = DragState_.DragStartAngleRadians;
		DragState_.AccumulatedDragAngleRadians = 0.0f;
	}
	else if (IsPlaneAxis(Axis))
	{
		FVector3 AxisU{};
		FVector3 AxisV{};
		FVector3 PlaneNormal{};
		GetWorldPlaneAxes(Axis, DragState_.GizmoRotation, &AxisU, &AxisV, &PlaneNormal);
		if (!RayPlaneIntersect(Ray, DragState_.GizmoOrigin, PlaneNormal, &HitPoint))
		{
			DragState_.bIsDragging = false;
			return false;
		}
		const FVector3 Local = HitPoint - DragState_.GizmoOrigin;
		DragState_.DragStartAxisParam = FVector3(Local.Dot(AxisU), Local.Dot(AxisV), 0.0f);
	}
	else if (Axis == EGizmoAxis::Uniform)
	{
		float CenterScreenX = 0.0f;
		float CenterScreenY = 0.0f;
		if (GizmoMode_ == EGizmoMode::Scale &&
			TryProjectWorldToScreen(
				Matrices,
				DragState_.GizmoOrigin,
				InViewportWidth,
				InViewportHeight,
				InNearPlane,
				InFarPlane,
				&CenterScreenX,
				&CenterScreenY,
				true))
		{
			DragState_.bUniformScreenScaleDrag = true;
			DragState_.DragStartUniformScreenRadius = std::max(
				std::hypot(InScreenX - CenterScreenX, InScreenY - CenterScreenY),
				kMinUniformScreenRadius);
		}
		else
		{
			FVector3 PlaneNormal = DragState_.GizmoOrigin -
				FVector3(InCamera.position.x, InCamera.position.y, InCamera.position.z);
			if (PlaneNormal.Length() < 1e-3f)
			{
				PlaneNormal = FVector3(0.0f, 0.0f, 1.0f);
			}
			PlaneNormal = PlaneNormal.Normalized();
			if (!RayPlaneIntersect(Ray, DragState_.GizmoOrigin, PlaneNormal, &HitPoint))
			{
				DragState_.bIsDragging = false;
				return false;
			}
			DragState_.DragStartAxisParam = HitPoint;
		}
	}
	else
	{
		const float GizmoScale = ComputeGizmoScale(DragState_.GizmoOrigin, InCamera);
		if ((GizmoMode_ == EGizmoMode::Translate || GizmoMode_ == EGizmoMode::Scale) &&
			TrySetupScreenLineDrag(
				Matrices,
				DragState_.GizmoOrigin,
				DragState_.AxisDirection,
				GizmoScale,
				InViewportWidth,
				InViewportHeight,
				InNearPlane,
				InFarPlane,
				InScreenX,
				InScreenY,
				&DragState_))
		{
			return true;
		}

		if (!ComputeLineAxisDragHitPoint(
			Ray, DragState_.GizmoOrigin, DragState_.AxisDirection, InCamera, &HitPoint))
		{
			DragState_.bIsDragging = false;
			return false;
		}
		DragState_.DragStartAxisParam = ProjectPointOnAxis(HitPoint, DragState_.GizmoOrigin, DragState_.AxisDirection);
	}
	return true;
}

void FEditorTransformGizmo::UpdateDrag(
	const AActor* InActor,
	const Dx12Renderer::CameraState& InCamera,
	UINT InViewportWidth,
	UINT InViewportHeight,
	float InNearPlane,
	float InFarPlane,
	float InScreenX,
	float InScreenY,
	FActorTransform* OutTransform)
{
	if (!DragState_.bIsDragging || OutTransform == nullptr || InActor == nullptr)
	{
		return;
	}

	*OutTransform = DragState_.DragStartTransform;

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

	FVector3 HitPoint{};
	if (GizmoMode_ == EGizmoMode::Translate)
	{
		if (IsPlaneAxis(DragState_.ActiveAxis))
		{
			FVector3 AxisU{};
			FVector3 AxisV{};
			FVector3 PlaneNormal{};
			GetWorldPlaneAxes(DragState_.ActiveAxis, DragState_.GizmoRotation, &AxisU, &AxisV, &PlaneNormal);
			if (!RayPlaneIntersect(Ray, DragState_.GizmoOrigin, PlaneNormal, &HitPoint))
			{
				return;
			}
			const FVector3 Local = HitPoint - DragState_.GizmoOrigin;
			const FVector3 Delta(
				Local.Dot(AxisU) - DragState_.DragStartAxisParam.X,
				Local.Dot(AxisV) - DragState_.DragStartAxisParam.Y,
				0.0f);
			OutTransform->Position =
				DragState_.DragStartTransform.Position + AxisU * Delta.X + AxisV * Delta.Y;
		}
		else if (DragState_.ActiveAxis == EGizmoAxis::Uniform)
		{
			FVector3 PlaneNormal = DragState_.GizmoOrigin -
				FVector3(InCamera.position.x, InCamera.position.y, InCamera.position.z);
			if (PlaneNormal.Length() < 1e-3f)
			{
				PlaneNormal = FVector3(0.0f, 0.0f, 1.0f);
			}
			PlaneNormal = PlaneNormal.Normalized();
			if (!RayPlaneIntersect(Ray, DragState_.GizmoOrigin, PlaneNormal, &HitPoint))
			{
				return;
			}
			const FVector3 Delta = HitPoint - DragState_.DragStartAxisParam;
			OutTransform->Position = DragState_.DragStartTransform.Position + Delta;
		}
		else
		{
			FVector3 Delta{};
			if (DragState_.bScreenLineDragActive)
			{
				const float ScreenDeltaX = InScreenX - DragState_.DragStartScreenX;
				const float ScreenDeltaY = InScreenY - DragState_.DragStartScreenY;
				const float ScreenAlong =
					ScreenDeltaX * DragState_.ScreenAxisDirX + ScreenDeltaY * DragState_.ScreenAxisDirY;
				const float WorldAlong = ScreenAlong * DragState_.WorldUnitsPerScreenPixelAlongAxis;
				Delta = DragState_.AxisDirection.Normalized() * WorldAlong;
			}
			else
			{
				if (!ComputeLineAxisDragHitPoint(
					Ray, DragState_.GizmoOrigin, DragState_.AxisDirection, InCamera, &HitPoint))
				{
					return;
				}
				const FVector3 CurrentParam =
					ProjectPointOnAxis(HitPoint, DragState_.GizmoOrigin, DragState_.AxisDirection);
				Delta = CurrentParam - DragState_.DragStartAxisParam;
			}
			OutTransform->Position = DragState_.DragStartTransform.Position + Delta;
		}
	}
	else if (GizmoMode_ == EGizmoMode::Scale)
	{
		if (DragState_.ActiveAxis == EGizmoAxis::Uniform)
		{
			float ScaleFactor = 1.0f;
			if (DragState_.bUniformScreenScaleDrag)
			{
				float CenterScreenX = 0.0f;
				float CenterScreenY = 0.0f;
				if (!TryProjectWorldToScreen(
					Matrices,
					DragState_.GizmoOrigin,
					InViewportWidth,
					InViewportHeight,
					InNearPlane,
					InFarPlane,
					&CenterScreenX,
					&CenterScreenY,
					true))
				{
					return;
				}
				const float StartRadius = std::max(DragState_.DragStartUniformScreenRadius, kMinUniformScreenRadius);
				const float CurrentRadius = std::max(
					std::hypot(InScreenX - CenterScreenX, InScreenY - CenterScreenY),
					kMinUniformScreenRadius);
				ScaleFactor = std::max(0.01f, CurrentRadius / StartRadius);
			}
			else
			{
				FVector3 PlaneNormal = DragState_.GizmoOrigin -
					FVector3(InCamera.position.x, InCamera.position.y, InCamera.position.z);
				if (PlaneNormal.Length() < 1e-3f)
				{
					PlaneNormal = FVector3(0.0f, 0.0f, 1.0f);
				}
				PlaneNormal = PlaneNormal.Normalized();
				if (!RayPlaneIntersect(Ray, DragState_.GizmoOrigin, PlaneNormal, &HitPoint))
				{
					return;
				}
				const float StartRadius = std::max(
					(DragState_.DragStartAxisParam - DragState_.GizmoOrigin).Length(),
					1e-4f);
				const float CurrentRadius = std::max((HitPoint - DragState_.GizmoOrigin).Length(), 1e-4f);
				ScaleFactor = std::max(0.01f, CurrentRadius / StartRadius);
			}
			OutTransform->Scale.X = std::max(
				kMinScaleValue, DragState_.DragStartTransform.Scale.X * ScaleFactor);
			OutTransform->Scale.Y = std::max(
				kMinScaleValue, DragState_.DragStartTransform.Scale.Y * ScaleFactor);
			OutTransform->Scale.Z = std::max(
				kMinScaleValue, DragState_.DragStartTransform.Scale.Z * ScaleFactor);
		}
		else
		{
			float SignedWorldAlong = 0.0f;
			if (DragState_.bScreenLineDragActive)
			{
				const float ScreenDeltaX = InScreenX - DragState_.DragStartScreenX;
				const float ScreenDeltaY = InScreenY - DragState_.DragStartScreenY;
				const float ScreenAlong =
					ScreenDeltaX * DragState_.ScreenAxisDirX + ScreenDeltaY * DragState_.ScreenAxisDirY;
				SignedWorldAlong = ScreenAlong * DragState_.WorldUnitsPerScreenPixelAlongAxis;
			}
			else if (ComputeLineAxisDragHitPoint(
				Ray, DragState_.GizmoOrigin, DragState_.AxisDirection, InCamera, &HitPoint))
			{
				const FVector3 CurrentParam =
					ProjectPointOnAxis(HitPoint, DragState_.GizmoOrigin, DragState_.AxisDirection);
				SignedWorldAlong =
					(CurrentParam - DragState_.DragStartAxisParam).Dot(DragState_.AxisDirection.Normalized());
			}
			else
			{
				return;
			}

			const float ScaleFactor = std::max(0.01f, 1.0f + SignedWorldAlong * kScaleAxisDragSensitivity);
			if (DragState_.ActiveAxis == EGizmoAxis::X)
			{
				OutTransform->Scale.X =
					std::max(kMinScaleValue, DragState_.DragStartTransform.Scale.X * ScaleFactor);
			}
			else if (DragState_.ActiveAxis == EGizmoAxis::Y)
			{
				OutTransform->Scale.Y =
					std::max(kMinScaleValue, DragState_.DragStartTransform.Scale.Y * ScaleFactor);
			}
			else if (DragState_.ActiveAxis == EGizmoAxis::Z)
			{
				OutTransform->Scale.Z =
					std::max(kMinScaleValue, DragState_.DragStartTransform.Scale.Z * ScaleFactor);
			}
		}
	}
	else if (GizmoMode_ == EGizmoMode::Rotate)
	{
		if (!RayPlaneIntersect(Ray, DragState_.GizmoOrigin, DragState_.AxisDirection, &HitPoint))
		{
			return;
		}
		const FVector3 Offset = HitPoint - DragState_.GizmoOrigin;
		const float Angle = ComputeRotateRingAngleRadians(
			Offset, DragState_.RingTangentU, DragState_.RingTangentV);
		const float StepDeltaRadians = NormalizeAngleDeltaShortest(Angle - DragState_.PreviousDragAngleRadians);
		DragState_.AccumulatedDragAngleRadians += StepDeltaRadians;
		DragState_.PreviousDragAngleRadians = Angle;
		DragState_.CurrentDragAngleRadians =
			DragState_.DragStartAngleRadians + DragState_.AccumulatedDragAngleRadians;
		const float SignedDeltaRadians = ComputeSignedRotateDragDeltaRadians(
			DragState_.ActiveAxis, DragState_.AccumulatedDragAngleRadians);

		OutTransform->Rotation = FTransform::RotateByWorldAxis(
			DragState_.DragStartTransform.Rotation,
			DragState_.AxisDirection,
			SignedDeltaRadians).GetNormalized();
	}
}

void FEditorTransformGizmo::EndDrag()
{
	DragState_ = FGizmoDragState{};
}

void FEditorTransformGizmo::BuildMeshVertices(
	const AActor* InActor,
	const Dx12Renderer::CameraState& InCamera,
	std::vector<Dx12Renderer::Vertex>* OutVertices) const
{
	if (OutVertices == nullptr || InActor == nullptr)
	{
		return;
	}

	OutVertices->clear();
	const FVector3 Origin = GetGizmoOrigin(InActor);
	const FRotator3 GizmoRotation = GetGizmoRotation(InActor);
	const float Scale = ComputeGizmoScale(Origin, InCamera);
	const EGizmoAxis ActiveHighlight = DragState_.bIsDragging ? DragState_.ActiveAxis : HoveredAxis_;
	const FVector3 GizmoAxisX = GetWorldAxisDirection(EGizmoAxis::X, GizmoRotation);
	const FVector3 GizmoAxisY = GetWorldAxisDirection(EGizmoAxis::Y, GizmoRotation);
	const FVector3 GizmoAxisZ = GetWorldAxisDirection(EGizmoAxis::Z, GizmoRotation);

	auto DrawTranslateAxis = [&](EGizmoAxis InAxis)
	{
		const float Alpha = (ActiveHighlight == InAxis) ? 1.0f : 0.85f;
		AppendAxisArrow(
			OutVertices,
			Origin,
			GetWorldAxisDirection(InAxis, GizmoRotation),
			Scale,
			AxisColor(InAxis, Alpha));
	};

	auto DrawScaleAxis = [&](EGizmoAxis InAxis)
	{
		const float Alpha = (ActiveHighlight == InAxis) ? 1.0f : 0.85f;
		AppendAxisScaleHandle(
			OutVertices,
			Origin,
			GetWorldAxisDirection(InAxis, GizmoRotation),
			GizmoAxisX,
			GizmoAxisY,
			GizmoAxisZ,
			Scale,
			AxisColor(InAxis, Alpha));
	};

	auto DrawPlane = [&](EGizmoAxis InPlaneAxis)
	{
		FVector3 AxisU{};
		FVector3 AxisV{};
		FVector3 PlaneNormal{};
		GetWorldPlaneAxes(InPlaneAxis, GizmoRotation, &AxisU, &AxisV, &PlaneNormal);
		const float Alpha = (ActiveHighlight == InPlaneAxis) ? 0.75f : 0.45f;
		AppendPlaneQuad(
			OutVertices,
			Origin,
			AxisU,
			AxisV,
			Scale * kPlaneHandleSizeFactor,
			AxisColor(InPlaneAxis, Alpha));
	};

	if (GizmoMode_ == EGizmoMode::Translate)
	{
		DrawPlane(EGizmoAxis::XY);
		DrawPlane(EGizmoAxis::YZ);
		DrawPlane(EGizmoAxis::XZ);
		DrawTranslateAxis(EGizmoAxis::X);
		DrawTranslateAxis(EGizmoAxis::Y);
		DrawTranslateAxis(EGizmoAxis::Z);
		const float CenterAlpha = (ActiveHighlight == EGizmoAxis::Uniform) ? 1.0f : 0.9f;
		AppendOrientedCenterBox(
			OutVertices,
			Origin,
			Scale * kCenterBoxHalfExtentFactor,
			GizmoAxisX,
			GizmoAxisY,
			GizmoAxisZ,
			DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, CenterAlpha));
	}
	else if (GizmoMode_ == EGizmoMode::Scale)
	{
		DrawScaleAxis(EGizmoAxis::X);
		DrawScaleAxis(EGizmoAxis::Y);
		DrawScaleAxis(EGizmoAxis::Z);
		const float ConnectorRadius = Scale * kScaleConnectorRadiusFactor;
		const FVector3 EndX = Origin + GizmoAxisX * Scale;
		const FVector3 EndY = Origin + GizmoAxisY * Scale;
		const FVector3 EndZ = Origin + GizmoAxisZ * Scale;
		AppendScaleConnectorLines(
			OutVertices,
			EndX,
			EndY,
			EndZ,
			ConnectorRadius,
			AxisColor(EGizmoAxis::X, 0.55f),
			AxisColor(EGizmoAxis::Z, 0.55f),
			AxisColor(EGizmoAxis::Y, 0.55f));
		const float CenterAlpha = (ActiveHighlight == EGizmoAxis::Uniform) ? 1.0f : 0.9f;
		AppendOrientedCenterBox(
			OutVertices,
			Origin,
			Scale * kCenterBoxHalfExtentFactor,
			GizmoAxisX,
			GizmoAxisY,
			GizmoAxisZ,
			DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, CenterAlpha));
	}
	else if (GizmoMode_ == EGizmoMode::Rotate)
	{
		const float TubeRadius = Scale * kRotateTubeRadiusFactor;
		const bool bRotateDragActive =
			DragState_.bIsDragging &&
			(DragState_.ActiveAxis == EGizmoAxis::X ||
			 DragState_.ActiveAxis == EGizmoAxis::Y ||
			 DragState_.ActiveAxis == EGizmoAxis::Z);

		if (bRotateDragActive)
		{
			const EGizmoAxis Axis = DragState_.ActiveAxis;
			const float Alpha = 1.0f;
			const DirectX::XMFLOAT4 Color = AxisColor(Axis, Alpha);
			for (int32_t Segment = 0; Segment < kRotateFullRingSegments; ++Segment)
			{
				const float Angle0 = (2.0f * kPi * static_cast<float>(Segment)) / static_cast<float>(kRotateFullRingSegments);
				const float Angle1 = (2.0f * kPi * static_cast<float>(Segment + 1)) / static_cast<float>(kRotateFullRingSegments);
				const FVector3 Point0 =
					Origin + RotateGizmoDirection(GizmoRotation, BuildLocalRingPoint(Axis, Angle0, Scale));
				const FVector3 Point1 =
					Origin + RotateGizmoDirection(GizmoRotation, BuildLocalRingPoint(Axis, Angle1, Scale));
				AppendCylinder(OutVertices, Point0, Point1, TubeRadius, Color);
			}
			AppendRotateDragVisuals(OutVertices, DragState_, Scale, Axis);
		}
		else
		{
			const FVector3 CameraPos(InCamera.position.x, InCamera.position.y, InCamera.position.z);
			int32_t OctantSignX = 1;
			int32_t OctantSignY = 1;
			int32_t OctantSignZ = 1;
			GetRotateGizmoOctant(Origin, GizmoRotation, CameraPos, &OctantSignX, &OctantSignY, &OctantSignZ);
			for (int32_t Ring = 0; Ring < 3; ++Ring)
			{
				const EGizmoAxis Axis = (Ring == 0) ? EGizmoAxis::X : ((Ring == 1) ? EGizmoAxis::Y : EGizmoAxis::Z);
				const float Alpha = (ActiveHighlight == Axis) ? 1.0f : 0.85f;
				const DirectX::XMFLOAT4 Color = AxisColor(Axis, Alpha);
				float ArcStartAngle = 0.0f;
				float ArcEndAngle = 0.0f;
				GetRotateRingArcAnglesForOctant(
					Axis, OctantSignX, OctantSignY, OctantSignZ, &ArcStartAngle, &ArcEndAngle);
				for (int32_t Segment = 0; Segment < kRotateQuarterRingSegments; ++Segment)
				{
					const float T0 = static_cast<float>(Segment) / static_cast<float>(kRotateQuarterRingSegments);
					const float T1 = static_cast<float>(Segment + 1) / static_cast<float>(kRotateQuarterRingSegments);
					const float Angle0 = ArcStartAngle + (ArcEndAngle - ArcStartAngle) * T0;
					const float Angle1 = ArcStartAngle + (ArcEndAngle - ArcStartAngle) * T1;
					const FVector3 Point0 =
						Origin + RotateGizmoDirection(GizmoRotation, BuildLocalRingPoint(Axis, Angle0, Scale));
					const FVector3 Point1 =
						Origin + RotateGizmoDirection(GizmoRotation, BuildLocalRingPoint(Axis, Angle1, Scale));
					AppendCylinder(OutVertices, Point0, Point1, TubeRadius, Color);
				}
			}
		}
	}
}

void FEditorTransformGizmo::BuildRotateDragLabel(
	const Dx12Renderer::CameraState& InCamera,
	UINT InViewportWidth,
	UINT InViewportHeight,
	float InNearPlane,
	float InFarPlane,
	FEditorRotateDragLabel* OutLabel) const
{
	if (OutLabel == nullptr)
	{
		return;
	}

	*OutLabel = FEditorRotateDragLabel{};
	if (!DragState_.bIsDragging || GizmoMode_ != EGizmoMode::Rotate || !IsLineAxis(DragState_.ActiveAxis))
	{
		return;
	}

	const float DeltaRadians = DragState_.AccumulatedDragAngleRadians;
	OutLabel->AngleDegrees =
		ComputeSignedRotateDragDeltaRadians(DragState_.ActiveAxis, DeltaRadians) * (180.0f / kPi);
	const float MidAngle = DragState_.DragStartAngleRadians + DeltaRadians * 0.5f;
	const float Scale = ComputeGizmoScale(DragState_.GizmoOrigin, InCamera);
	const FVector3 LabelWorldPos = BuildRingPointFromTangentAngles(
		DragState_.GizmoOrigin,
		DragState_.RingTangentU,
		DragState_.RingTangentV,
		MidAngle,
		Scale) + DragState_.AxisDirection * (Scale * 0.14f);

	const FEditorViewportMatrices Matrices = FEditorViewMatrices::Build(
		InCamera, InViewportWidth, InViewportHeight, InNearPlane, InFarPlane);
	float ScreenX = 0.0f;
	float ScreenY = 0.0f;
	if (!TryProjectWorldToScreen(
		Matrices,
		LabelWorldPos,
		InViewportWidth,
		InViewportHeight,
		InNearPlane,
		InFarPlane,
		&ScreenX,
		&ScreenY))
	{
		return;
	}

	OutLabel->bVisible = true;
	OutLabel->ScreenX = ScreenX;
	OutLabel->ScreenY = ScreenY;
}
