#pragma once

#include <vector>

#include "editor/EditorTypes.h"
#include "math/FRotator3.h"
#include "render/Dx12Renderer.h"
#include "world/ActorTransform.h"

class AActor;

class FEditorTransformGizmo
{
public:
	EGizmoMode GetMode() const;
	void SetMode(EGizmoMode InMode);
	bool IsDragging() const;

	void SetHoveredAxis(EGizmoAxis InAxis);
	EGizmoAxis GetHoveredAxis() const;

	bool TryBeginDrag(
		const AActor* InActor,
		const Dx12Renderer::CameraState& InCamera,
		UINT InViewportWidth,
		UINT InViewportHeight,
		float InNearPlane,
		float InFarPlane,
		float InScreenX,
		float InScreenY,
		EGizmoAxis InPreferredAxis = EGizmoAxis::None,
		float InPickToleranceScale = 1.0f);

	void UpdateDrag(
		const AActor* InActor,
		const Dx12Renderer::CameraState& InCamera,
		UINT InViewportWidth,
		UINT InViewportHeight,
		float InNearPlane,
		float InFarPlane,
		float InScreenX,
		float InScreenY,
		FActorTransform* OutTransform);

	void EndDrag();

	EGizmoAxis HitTest(
		const AActor* InActor,
		const Dx12Renderer::CameraState& InCamera,
		UINT InViewportWidth,
		UINT InViewportHeight,
		float InNearPlane,
		float InFarPlane,
		float InScreenX,
		float InScreenY,
		float InPickToleranceScale = 1.0f) const;

	void BuildMeshVertices(
		const AActor* InActor,
		const Dx12Renderer::CameraState& InCamera,
		std::vector<Dx12Renderer::Vertex>* OutVertices) const;

	void BuildRotateDragLabel(
		const Dx12Renderer::CameraState& InCamera,
		UINT InViewportWidth,
		UINT InViewportHeight,
		float InNearPlane,
		float InFarPlane,
		FEditorRotateDragLabel* OutLabel) const;

	bool IsScreenPointNearGizmo(
		const AActor* InActor,
		const Dx12Renderer::CameraState& InCamera,
		UINT InViewportWidth,
		UINT InViewportHeight,
		float InNearPlane,
		float InFarPlane,
		float InScreenX,
		float InScreenY,
		float InScreenSlopPixels = 24.0f) const;

private:
	float ComputeGizmoScale(const FVector3& InGizmoOrigin, const Dx12Renderer::CameraState& InCamera) const;
	FVector3 GetGizmoOrigin(const AActor* InActor) const;
	FRotator3 GetGizmoRotation(const AActor* InActor) const;
	FVector3 GetLocalAxisDirection(EGizmoAxis InAxis) const;
	FVector3 GetWorldAxisDirection(EGizmoAxis InAxis, const FRotator3& InGizmoRotation) const;
	bool IsPlaneAxis(EGizmoAxis InAxis) const;
	bool IsLineAxis(EGizmoAxis InAxis) const;
	void GetWorldPlaneAxes(
		EGizmoAxis InPlaneAxis,
		const FRotator3& InGizmoRotation,
		FVector3* OutAxisU,
		FVector3* OutAxisV,
		FVector3* OutPlaneNormal) const;
	bool RayHitCylinder(
		const FWorldRay& InRay,
		const FVector3& InSegmentStart,
		const FVector3& InSegmentEnd,
		float InRadius,
		float* OutRayT) const;
	bool RayHitPlaneQuad(
		const FWorldRay& InRay,
		const FVector3& InOrigin,
		const FVector3& InAxisU,
		const FVector3& InAxisV,
		float InPlaneSize,
		float* OutRayT) const;
	bool RayHitCenterBox(
		const FWorldRay& InRay,
		const FVector3& InOrigin,
		float InHalfExtent,
		float* OutRayT) const;

	EGizmoMode GizmoMode_ = EGizmoMode::Translate;
	EGizmoAxis HoveredAxis_ = EGizmoAxis::None;
	FGizmoDragState DragState_{};
};
