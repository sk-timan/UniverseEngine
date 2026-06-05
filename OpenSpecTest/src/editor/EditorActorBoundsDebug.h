#pragma once

#include <DirectXMath.h>
#include <vector>

#include "math/FVector3.h"
#include "render/Dx12Renderer.h"

class AActor;

struct FEditorWorldAabb
{
	FVector3 Min{};
	FVector3 Max{};
	bool bIsValid = false;
};

struct FEditorWorldObb
{
	FVector3 Center{};
	FVector3 AxisX{};
	FVector3 AxisY{};
	FVector3 AxisZ{};
	FVector3 HalfExtents{};
	bool bIsValid = false;
};

class FEditorActorBoundsDebug
{
public:
	static bool ComputeActorWorldAabb(const AActor* InActor, FEditorWorldAabb* OutAabb);

	static void AppendWorldAabbWireframe(
		const FEditorWorldAabb& InAabb,
		const DirectX::XMFLOAT4& InColor,
		std::vector<Dx12Renderer::Vertex>* OutVertices);

	static void AppendWorldObbWireframe(
		const FEditorWorldObb& InObb,
		const DirectX::XMFLOAT4& InColor,
		std::vector<Dx12Renderer::Vertex>* OutVertices);

	static void AppendActorWorldObbWireframes(
		const AActor* InActor,
		const DirectX::XMFLOAT4& InColor,
		std::vector<Dx12Renderer::Vertex>* OutVertices);

	static void AppendActorWorldSectionBoundsWireframes(
		const AActor* InActor,
		const DirectX::XMFLOAT4& InColor,
		std::vector<Dx12Renderer::Vertex>* OutVertices);
};
