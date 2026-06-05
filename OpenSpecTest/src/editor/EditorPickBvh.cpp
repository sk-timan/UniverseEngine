#include "editor/EditorPickBvh.h"

#include <algorithm>
#include <limits>

#include "editor/EditorActorBoundsDebug.h"
#include "world/Actor.h"
#include "world/Level.h"

void FEditorPickBvh::Clear()
{
	Nodes_.clear();
}

bool FEditorPickBvh::IsBuilt() const
{
	return !Nodes_.empty();
}

void FEditorPickBvh::Build(const ULevel* InLevel)
{
	Clear();
	if (InLevel == nullptr)
	{
		return;
	}

	std::vector<FBuildItem> BuildItems;
	BuildItems.reserve(InLevel->GetActorCount());
	for (uint64_t ActorObjectId : InLevel->GetActorObjectIds())
	{
		const AActor* Actor = InLevel->FindActor(ActorObjectId);
		if (Actor == nullptr || Actor->IsPendingDestroy() || !Actor->IsPickable())
		{
			continue;
		}

		FEditorWorldAabb WorldAabb{};
		if (!FEditorActorBoundsDebug::ComputeActorWorldAabb(Actor, &WorldAabb))
		{
			continue;
		}

		FBuildItem Item{};
		Item.ActorObjectId = ActorObjectId;
		Item.Bounds.Min = WorldAabb.Min;
		Item.Bounds.Max = WorldAabb.Max;
		Item.Centroid = FVector3(
			(WorldAabb.Min.X + WorldAabb.Max.X) * 0.5f,
			(WorldAabb.Min.Y + WorldAabb.Max.Y) * 0.5f,
			(WorldAabb.Min.Z + WorldAabb.Max.Z) * 0.5f);
		BuildItems.push_back(Item);
	}

	if (BuildItems.empty())
	{
		return;
	}

	Nodes_.reserve(BuildItems.size() * 2);
	(void)BuildRecursive(BuildItems, 0, static_cast<int32_t>(BuildItems.size()));
}

int32_t FEditorPickBvh::BuildRecursive(std::vector<FBuildItem>& InOutItems, int32_t InStart, int32_t InCount)
{
	const int32_t NodeIndex = static_cast<int32_t>(Nodes_.size());
	Nodes_.push_back(FNode{});

	if (InCount <= 1)
	{
		FNode& LeafNode = Nodes_[NodeIndex];
		LeafNode.bIsLeaf = true;
		LeafNode.Bounds = InOutItems[InStart].Bounds;
		LeafNode.ActorObjectId = InOutItems[InStart].ActorObjectId;
		return NodeIndex;
	}

	FEditorAxisAlignedBox NodeBounds = InOutItems[InStart].Bounds;
	for (int32_t Index = InStart + 1; Index < InStart + InCount; ++Index)
	{
		const FEditorAxisAlignedBox& ItemBounds = InOutItems[Index].Bounds;
		NodeBounds.Min.X = std::min(NodeBounds.Min.X, ItemBounds.Min.X);
		NodeBounds.Min.Y = std::min(NodeBounds.Min.Y, ItemBounds.Min.Y);
		NodeBounds.Min.Z = std::min(NodeBounds.Min.Z, ItemBounds.Min.Z);
		NodeBounds.Max.X = std::max(NodeBounds.Max.X, ItemBounds.Max.X);
		NodeBounds.Max.Y = std::max(NodeBounds.Max.Y, ItemBounds.Max.Y);
		NodeBounds.Max.Z = std::max(NodeBounds.Max.Z, ItemBounds.Max.Z);
	}

	const float ExtentX = NodeBounds.Max.X - NodeBounds.Min.X;
	const float ExtentY = NodeBounds.Max.Y - NodeBounds.Min.Y;
	const float ExtentZ = NodeBounds.Max.Z - NodeBounds.Min.Z;
	int32_t SplitAxis = 0;
	if (ExtentY >= ExtentX && ExtentY >= ExtentZ)
	{
		SplitAxis = 1;
	}
	else if (ExtentZ >= ExtentX && ExtentZ >= ExtentY)
	{
		SplitAxis = 2;
	}

	const int32_t Mid = InStart + InCount / 2;
	std::nth_element(
		InOutItems.begin() + InStart,
		InOutItems.begin() + Mid,
		InOutItems.begin() + InStart + InCount,
		[SplitAxis](const FBuildItem& InLeft, const FBuildItem& InRight)
		{
			const float LeftValue = (SplitAxis == 0)
				? InLeft.Centroid.X
				: ((SplitAxis == 1) ? InLeft.Centroid.Y : InLeft.Centroid.Z);
			const float RightValue = (SplitAxis == 0)
				? InRight.Centroid.X
				: ((SplitAxis == 1) ? InRight.Centroid.Y : InRight.Centroid.Z);
			return LeftValue < RightValue;
		});

	FNode& InternalNode = Nodes_[NodeIndex];
	InternalNode.bIsLeaf = false;
	InternalNode.Bounds = NodeBounds;
	InternalNode.LeftChild = BuildRecursive(InOutItems, InStart, Mid - InStart);
	InternalNode.RightChild = BuildRecursive(InOutItems, Mid, InStart + InCount - Mid);
	return NodeIndex;
}

void FEditorPickBvh::Query(
	const FWorldRay& InRay,
	float InMaxT,
	std::vector<FEditorPickBvhCandidate>* OutCandidates) const
{
	if (OutCandidates == nullptr || Nodes_.empty())
	{
		return;
	}

	OutCandidates->clear();
	std::vector<int32_t> NodeStack;
	NodeStack.push_back(0);

	while (!NodeStack.empty())
	{
		const int32_t NodeIndex = NodeStack.back();
		NodeStack.pop_back();
		if (NodeIndex < 0 || NodeIndex >= static_cast<int32_t>(Nodes_.size()))
		{
			continue;
		}

		const FNode& Node = Nodes_[NodeIndex];
		float EnterT = 0.0f;
		float ExitT = 0.0f;
		if (!FEditorRayIntersection::RayIntersectsAabb(InRay, Node.Bounds, &EnterT, &ExitT))
		{
			continue;
		}

		const float ClampedEnterT = (EnterT >= 0.0f) ? EnterT : 0.0f;
		if (ClampedEnterT > InMaxT)
		{
			continue;
		}

		if (Node.bIsLeaf)
		{
			FEditorPickBvhCandidate Candidate{};
			Candidate.ActorObjectId = Node.ActorObjectId;
			Candidate.EnterT = ClampedEnterT;
			OutCandidates->push_back(Candidate);
			continue;
		}

		float LeftEnterT = 0.0f;
		float RightEnterT = 0.0f;
		const bool bHitLeft = Node.LeftChild >= 0
			&& FEditorRayIntersection::RayIntersectsAabb(
				InRay,
				Nodes_[Node.LeftChild].Bounds,
				&LeftEnterT,
				nullptr);
		const bool bHitRight = Node.RightChild >= 0
			&& FEditorRayIntersection::RayIntersectsAabb(
				InRay,
				Nodes_[Node.RightChild].Bounds,
				&RightEnterT,
				nullptr);

		if (bHitLeft && bHitRight)
		{
			const float LeftT = (LeftEnterT >= 0.0f) ? LeftEnterT : 0.0f;
			const float RightT = (RightEnterT >= 0.0f) ? RightEnterT : 0.0f;
			if (LeftT <= RightT)
			{
				NodeStack.push_back(Node.RightChild);
				NodeStack.push_back(Node.LeftChild);
			}
			else
			{
				NodeStack.push_back(Node.LeftChild);
				NodeStack.push_back(Node.RightChild);
			}
		}
		else if (bHitLeft)
		{
			NodeStack.push_back(Node.LeftChild);
		}
		else if (bHitRight)
		{
			NodeStack.push_back(Node.RightChild);
		}
	}

	std::sort(
		OutCandidates->begin(),
		OutCandidates->end(),
		[](const FEditorPickBvhCandidate& InLeft, const FEditorPickBvhCandidate& InRight)
		{
			return InLeft.EnterT < InRight.EnterT;
		});
}
