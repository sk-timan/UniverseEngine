#pragma once

#include <cstdint>

#include <DirectXMath.h>

#include "editor/EditorTypes.h"
#include "render/Dx12Renderer.h"

struct FEditorViewportMatrices
{
	DirectX::XMMATRIX View{};
	DirectX::XMMATRIX Projection{};
	DirectX::XMMATRIX ViewProjection{};
};

class FEditorViewMatrices
{
public:
	static constexpr float kCameraFovDegrees = 60.0f;

	static FEditorViewportMatrices Build(
		const Dx12Renderer::CameraState& InCamera,
		UINT InViewportWidth,
		UINT InViewportHeight,
		float InNearPlane,
		float InFarPlane);

	static FWorldRay BuildWorldRayFromScreen(
		const FEditorViewportMatrices& InMatrices,
		const DirectX::XMFLOAT3& InCameraWorldPosition,
		float InScreenX,
		float InScreenY,
		UINT InViewportWidth,
		UINT InViewportHeight,
		float InNearPlane,
		float InFarPlane);
};
