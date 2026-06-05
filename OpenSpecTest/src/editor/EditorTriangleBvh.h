#pragma once

#include <cstdint>
#include <vector>

#include "editor/EditorRayIntersection.h"
#include "editor/EditorTypes.h"
#include "math/FVector3.h"

class UStaticMesh;
class USkeletalMesh;

struct FEditorTriangleBvhQueryResult
{
	bool bHit = false;
	float HitT = 0.0f;
};

// Per-mesh triangle BVH for editor picking narrow phase.
class FEditorTriangleBvh
{
public:
	void Build(
		const std::vector<FVector3>& InLocalPositions,
		const std::vector<uint32_t>& InIndices,
		EPickTriangleBvhSplitMethod InSplitMethod);

	void Clear();
	bool IsBuilt() const;

	void Query(
		const FWorldRay& InLocalRay,
		uint32_t InFirstIndex,
		uint32_t InIndexCount,
		float InMaxT,
		FEditorTriangleBvhQueryResult* OutResult) const;

private:
	struct FBuildItem
	{
		int32_t TriangleIndex = -1;
		FEditorAxisAlignedBox Bounds{};
		FVector3 Centroid{};
	};

	struct FNode
	{
		FEditorAxisAlignedBox Bounds{};
		int32_t LeftChild = -1;
		int32_t RightChild = -1;
		int32_t FirstTriangle = -1;
		int32_t TriangleCount = 0;
		bool bIsLeaf = false;
	};

	static FEditorAxisAlignedBox ComputeBoundsForItems(
		const std::vector<FBuildItem>& InItems,
		int32_t InStart,
		int32_t InCount);
	static void PartitionByAxis(
		std::vector<FBuildItem>& InOutItems,
		int32_t InStart,
		int32_t InCount,
		int32_t InAxis,
		float InSplitValue,
		int32_t* OutLeftCount);
	static float EvaluateSahSplitCost(
		const std::vector<FBuildItem>& InItems,
		int32_t InStart,
		int32_t InCount,
		int32_t InAxis,
		float InSplitValue,
		const FEditorAxisAlignedBox& InParentBounds);
	int32_t BuildRecursiveImpl(
		std::vector<FBuildItem>& InOutItems,
		int32_t InStart,
		int32_t InCount,
		EPickTriangleBvhSplitMethod InSplitMethod);

	std::vector<FNode> Nodes_;
	std::vector<int32_t> TriangleIndices_;
	std::vector<FVector3> LocalPositions_;
	std::vector<uint32_t> MeshIndices_;
};

class FEditorTriangleBvhCache
{
public:
	const FEditorTriangleBvh* GetOrBuild(
		const UStaticMesh* InStaticMesh,
		EPickTriangleBvhSplitMethod InSplitMethod);
	const FEditorTriangleBvh* GetOrBuild(
		const USkeletalMesh* InSkeletalMesh,
		EPickTriangleBvhSplitMethod InSplitMethod);

	void InvalidateAll();

private:
	struct FCacheEntry
	{
		const void* Mesh = nullptr;
		EPickTriangleBvhSplitMethod SplitMethod = EPickTriangleBvhSplitMethod::Median;
		size_t VertexCount = 0;
		size_t IndexCount = 0;
		FEditorTriangleBvh Bvh;
	};

	std::vector<FCacheEntry> Entries_;
};
