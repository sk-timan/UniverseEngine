#pragma once

#include <cstdint>
#include <vector>

#include "editor/EditorRayIntersection.h"
#include "editor/EditorTypes.h"

class ULevel;

struct FEditorPickBvhCandidate
{
	uint64_t ActorObjectId = 0;
	float EnterT = 0.0f;
};

// Bounding Volume Hierarchy for editor actor picking broad phase.
// Each leaf stores one Actor's world AABB; ray traversal returns candidates sorted by enter T.
class FEditorPickBvh
{
public:
	void Build(const ULevel* InLevel);
	void Clear();

	bool IsBuilt() const;
	void Query(const FWorldRay& InRay, float InMaxT, std::vector<FEditorPickBvhCandidate>* OutCandidates) const;

private:
	struct FBuildItem
	{
		uint64_t ActorObjectId = 0;
		FEditorAxisAlignedBox Bounds{};
		FVector3 Centroid{};
	};

	struct FNode
	{
		FEditorAxisAlignedBox Bounds{};
		int32_t LeftChild = -1;
		int32_t RightChild = -1;
		uint64_t ActorObjectId = 0;
		bool bIsLeaf = false;
	};

	int32_t BuildRecursive(std::vector<FBuildItem>& InOutItems, int32_t InStart, int32_t InCount);

	std::vector<FNode> Nodes_;
};
