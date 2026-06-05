#pragma once

#include <cstdint>

#include "editor/EditorTypes.h"
#include "editor/EditorViewMatrices.h"
#include "render/Dx12Renderer.h"

class ULevel;

class FEditorPicking
{
public:
	static FEditorPickResult PickActor(
		const ULevel* InLevel,
		const Dx12Renderer::CameraState& InCamera,
		UINT InViewportWidth,
		UINT InViewportHeight,
		float InNearPlane,
		float InFarPlane,
		float InScreenX,
		float InScreenY,
		EPickTriangleBvhSplitMethod InTriangleBvhSplitMethod,
		uint32_t InSceneRevision = 0);

	static void InvalidateTriangleBvhCache();
};
