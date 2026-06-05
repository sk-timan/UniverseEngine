#include "editor/EditorViewMatrices.h"

#include <algorithm>

#include <DirectXMath.h>

using namespace DirectX;

namespace
{
constexpr float kRayEpsilon = 1e-6f;
}

FEditorViewportMatrices FEditorViewMatrices::Build(
	const Dx12Renderer::CameraState& InCamera,
	UINT InViewportWidth,
	UINT InViewportHeight,
	float InNearPlane,
	float InFarPlane)
{
	FEditorViewportMatrices Result;
	const float ClampedPitch = std::clamp(InCamera.pitch, -1.4f, 1.4f);
	const XMVECTOR Position = XMVectorSet(InCamera.position.x, InCamera.position.y, InCamera.position.z, 1.0f);
	const XMVECTOR Forward = XMVector3Normalize(
		XMVectorSet(
			std::cos(ClampedPitch) * std::cos(InCamera.yaw),
			std::cos(ClampedPitch) * std::sin(InCamera.yaw),
			std::sin(ClampedPitch),
			0.0f));
	const XMVECTOR Up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

	Result.View = XMMatrixLookToLH(Position, Forward, Up);
	const float Aspect = (InViewportHeight > 0)
		? (static_cast<float>(InViewportWidth) / static_cast<float>(InViewportHeight))
		: (16.0f / 9.0f);
	Result.Projection = XMMatrixPerspectiveFovLH(
		XMConvertToRadians(kCameraFovDegrees),
		Aspect,
		InNearPlane,
		InFarPlane);
	Result.ViewProjection = Result.View * Result.Projection;
	return Result;
}

FWorldRay FEditorViewMatrices::BuildWorldRayFromScreen(
	const FEditorViewportMatrices& InMatrices,
	const DirectX::XMFLOAT3& InCameraWorldPosition,
	float InScreenX,
	float InScreenY,
	UINT InViewportWidth,
	UINT InViewportHeight,
	float InNearPlane,
	float InFarPlane)
{
	FWorldRay Result;
	if (InViewportWidth == 0 || InViewportHeight == 0)
	{
		return Result;
	}

	const float PixelX = InScreenX + 0.5f;
	const float PixelY = InScreenY + 0.5f;
	const float ViewportWidth = static_cast<float>(InViewportWidth);
	const float ViewportHeight = static_cast<float>(InViewportHeight);

	const XMVECTOR CameraPos = XMVectorSet(
		InCameraWorldPosition.x,
		InCameraWorldPosition.y,
		InCameraWorldPosition.z,
		1.0f);
	const XMVECTOR WorldFar = XMVector3Unproject(
		XMVectorSet(PixelX, PixelY, 1.0f, 1.0f),
		0.0f,
		0.0f,
		ViewportWidth,
		ViewportHeight,
		InNearPlane,
		InFarPlane,
		InMatrices.Projection,
		InMatrices.View,
		XMMatrixIdentity());

	// Direction must aim from the camera through the clicked pixel. Using (WorldFar - WorldNear)
	// with a camera origin breaks when the near/far segment is parallel to an axis (dx≈0):
	// the ray stays at x=camera.x and misses geometry under the cursor.
	const XMVECTOR Dir = XMVector3Normalize(XMVectorSubtract(WorldFar, CameraPos));
	Result.Origin = FVector3(
		InCameraWorldPosition.x,
		InCameraWorldPosition.y,
		InCameraWorldPosition.z);
	Result.Direction = FVector3(
		XMVectorGetX(Dir),
		XMVectorGetY(Dir),
		XMVectorGetZ(Dir));
	if (Result.Direction.Length() < kRayEpsilon)
	{
		Result.Direction = FVector3(0.0f, 0.0f, -1.0f);
	}
	return Result;
}
