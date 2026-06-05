#pragma once

#include <cstdint>

#include "math/FVector3.h"
#include "math/FRotator3.h"
#include "world/ActorTransform.h"

enum class EGizmoMode
{
	Translate,
	Rotate,
	Scale,
};

enum class EGizmoAxis
{
	None,
	X,
	Y,
	Z,
	XY,
	YZ,
	XZ,
	Uniform,
};

struct FWorldRay
{
	FVector3 Origin{};
	FVector3 Direction{};
};

struct FEditorPickResult
{
	uint64_t ActorObjectId = 0;
	float HitDistance = 0.0f;
	bool bHit = false;
};

enum class EPickTriangleBvhSplitMethod
{
	Median = 0,
	Sah = 1,
};

struct FGizmoDragState
{
	bool bIsDragging = false;
	EGizmoAxis ActiveAxis = EGizmoAxis::None;
	FActorTransform DragStartTransform{};
	FVector3 DragStartAxisParam{};
	FVector3 GizmoOrigin{};
	FRotator3 GizmoRotation{};
	FVector3 AxisDirection{};
	FVector3 RingTangentU{};
	FVector3 RingTangentV{};
	float DragStartAngleRadians = 0.0f;
	float CurrentDragAngleRadians = 0.0f;
	float PreviousDragAngleRadians = 0.0f;
	float AccumulatedDragAngleRadians = 0.0f;
	bool bScreenLineDragActive = false;
	float DragStartScreenX = 0.0f;
	float DragStartScreenY = 0.0f;
	float ScreenAxisDirX = 0.0f;
	float ScreenAxisDirY = 0.0f;
	float WorldUnitsPerScreenPixelAlongAxis = 0.0f;
	bool bUniformScreenScaleDrag = false;
	float DragStartUniformScreenRadius = 0.0f;
};

struct FEditorRotateDragLabel
{
	bool bVisible = false;
	float ScreenX = 0.0f;
	float ScreenY = 0.0f;
	float AngleDegrees = 0.0f;
};
