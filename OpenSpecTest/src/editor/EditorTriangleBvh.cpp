#include "editor/EditorTriangleBvh.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "render/asset/SkeletalMesh.h"
#include "render/asset/StaticMesh.h"

namespace
{
constexpr int32_t kMaxTrianglesPerLeaf = 4;
constexpr float kSplitCostTraversal = 1.0f;
constexpr float kSplitCostIntersect = 1.0f;

float ComputeAabbSurfaceArea(const FEditorAxisAlignedBox& InBox)
{
	const float ExtentX = std::max(InBox.Max.X - InBox.Min.X, 0.0f);
	const float ExtentY = std::max(InBox.Max.Y - InBox.Min.Y, 0.0f);
	const float ExtentZ = std::max(InBox.Max.Z - InBox.Min.Z, 0.0f);
	return 2.0f * (ExtentX * ExtentY + ExtentX * ExtentZ + ExtentY * ExtentZ);
}

FEditorAxisAlignedBox UnionBounds(const FEditorAxisAlignedBox& InLeft, const FEditorAxisAlignedBox& InRight)
{
	FEditorAxisAlignedBox Result = InLeft;
	Result.Min.X = std::min(Result.Min.X, InRight.Min.X);
	Result.Min.Y = std::min(Result.Min.Y, InRight.Min.Y);
	Result.Min.Z = std::min(Result.Min.Z, InRight.Min.Z);
	Result.Max.X = std::max(Result.Max.X, InRight.Max.X);
	Result.Max.Y = std::max(Result.Max.Y, InRight.Max.Y);
	Result.Max.Z = std::max(Result.Max.Z, InRight.Max.Z);
	return Result;
}

int32_t ChooseLongestAxis(const FEditorAxisAlignedBox& InBounds)
{
	const float ExtentX = InBounds.Max.X - InBounds.Min.X;
	const float ExtentY = InBounds.Max.Y - InBounds.Min.Y;
	const float ExtentZ = InBounds.Max.Z - InBounds.Min.Z;
	if (ExtentY >= ExtentX && ExtentY >= ExtentZ)
	{
		return 1;
	}
	if (ExtentZ >= ExtentX && ExtentZ >= ExtentY)
	{
		return 2;
	}
	return 0;
}

float GetCentroidAxisValue(const FVector3& InCentroid, int32_t InAxis)
{
	if (InAxis == 0)
	{
		return InCentroid.X;
	}
	if (InAxis == 1)
	{
		return InCentroid.Y;
	}
	return InCentroid.Z;
}

float GetBoundsAxisValue(const FEditorAxisAlignedBox& InBounds, int32_t InAxis, bool bIsMax)
{
	if (InAxis == 0)
	{
		return bIsMax ? InBounds.Max.X : InBounds.Min.X;
	}
	if (InAxis == 1)
	{
		return bIsMax ? InBounds.Max.Y : InBounds.Min.Y;
	}
	return bIsMax ? InBounds.Max.Z : InBounds.Min.Z;
}

bool TriangleOverlapsIndexRange(
	int32_t InTriangleIndex,
	uint32_t InFirstIndex,
	uint32_t InIndexCount,
	uint32_t InMeshIndexCount)
{
	if (InIndexCount == 0)
	{
		return true;
	}

	const uint32_t TriangleFirstIndex = static_cast<uint32_t>(InTriangleIndex) * 3u;
	return TriangleFirstIndex >= InFirstIndex
		&& (TriangleFirstIndex + 3u) <= (InFirstIndex + InIndexCount)
		&& (TriangleFirstIndex + 3u) <= InMeshIndexCount;
}
} // namespace

void FEditorTriangleBvh::Clear()
{
	Nodes_.clear();
	TriangleIndices_.clear();
	LocalPositions_.clear();
	MeshIndices_.clear();
}

bool FEditorTriangleBvh::IsBuilt() const
{
	return !Nodes_.empty();
}

void FEditorTriangleBvh::Build(
	const std::vector<FVector3>& InLocalPositions,
	const std::vector<uint32_t>& InIndices,
	EPickTriangleBvhSplitMethod InSplitMethod)
{
	Clear();
	LocalPositions_ = InLocalPositions;
	MeshIndices_ = InIndices;

	if (InLocalPositions.empty() || InIndices.size() < 3)
	{
		return;
	}

	const int32_t TriangleCount = static_cast<int32_t>(InIndices.size() / 3);
	std::vector<FBuildItem> BuildItems;
	BuildItems.reserve(static_cast<size_t>(TriangleCount));
	for (int32_t TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
	{
		const uint32_t I0 = InIndices[static_cast<size_t>(TriangleIndex) * 3 + 0];
		const uint32_t I1 = InIndices[static_cast<size_t>(TriangleIndex) * 3 + 1];
		const uint32_t I2 = InIndices[static_cast<size_t>(TriangleIndex) * 3 + 2];
		if (I0 >= InLocalPositions.size()
			|| I1 >= InLocalPositions.size()
			|| I2 >= InLocalPositions.size())
		{
			continue;
		}

		const FVector3& V0 = InLocalPositions[I0];
		const FVector3& V1 = InLocalPositions[I1];
		const FVector3& V2 = InLocalPositions[I2];

		FBuildItem Item{};
		Item.TriangleIndex = TriangleIndex;
		Item.Centroid = (V0 + V1 + V2) * (1.0f / 3.0f);
		Item.Bounds.Min.X = std::min({V0.X, V1.X, V2.X});
		Item.Bounds.Min.Y = std::min({V0.Y, V1.Y, V2.Y});
		Item.Bounds.Min.Z = std::min({V0.Z, V1.Z, V2.Z});
		Item.Bounds.Max.X = std::max({V0.X, V1.X, V2.X});
		Item.Bounds.Max.Y = std::max({V0.Y, V1.Y, V2.Y});
		Item.Bounds.Max.Z = std::max({V0.Z, V1.Z, V2.Z});
		BuildItems.push_back(Item);
	}

	if (BuildItems.empty())
	{
		return;
	}

	TriangleIndices_.reserve(BuildItems.size());
	Nodes_.reserve(BuildItems.size() * 2);
	(void)BuildRecursiveImpl(BuildItems, 0, static_cast<int32_t>(BuildItems.size()), InSplitMethod);
}

FEditorAxisAlignedBox FEditorTriangleBvh::ComputeBoundsForItems(
	const std::vector<FBuildItem>& InItems,
	int32_t InStart,
	int32_t InCount)
{
	FEditorAxisAlignedBox Result = InItems[static_cast<size_t>(InStart)].Bounds;
	for (int32_t Index = InStart + 1; Index < InStart + InCount; ++Index)
	{
		Result = UnionBounds(Result, InItems[static_cast<size_t>(Index)].Bounds);
	}
	return Result;
}

void FEditorTriangleBvh::PartitionByAxis(
	std::vector<FBuildItem>& InOutItems,
	int32_t InStart,
	int32_t InCount,
	int32_t InAxis,
	float InSplitValue,
	int32_t* OutLeftCount)
{
	const int32_t Mid = InStart + InCount / 2;
	std::nth_element(
		InOutItems.begin() + InStart,
		InOutItems.begin() + Mid,
		InOutItems.begin() + InStart + InCount,
		[InAxis](const FBuildItem& InLeft, const FBuildItem& InRight)
		{
			const float LeftValue = GetCentroidAxisValue(InLeft.Centroid, InAxis);
			const float RightValue = GetCentroidAxisValue(InRight.Centroid, InAxis);
			if (LeftValue != RightValue)
			{
				return LeftValue < RightValue;
			}
			return InLeft.TriangleIndex < InRight.TriangleIndex;
		});

	int32_t LeftCount = Mid - InStart;
	if (LeftCount <= 0)
	{
		LeftCount = 1;
	}
	else if (LeftCount >= InCount)
	{
		LeftCount = InCount - 1;
	}
	*OutLeftCount = LeftCount;
}

float FEditorTriangleBvh::EvaluateSahSplitCost(
	const std::vector<FBuildItem>& InItems,
	int32_t InStart,
	int32_t InCount,
	int32_t InAxis,
	float InSplitValue,
	const FEditorAxisAlignedBox& InParentBounds)
{
	std::vector<FBuildItem> ScratchItems(
		InItems.begin() + InStart,
		InItems.begin() + InStart + InCount);
	int32_t LeftCount = 0;
	PartitionByAxis(ScratchItems, 0, InCount, InAxis, InSplitValue, &LeftCount);

	FEditorAxisAlignedBox LeftBounds = ScratchItems[0].Bounds;
	for (int32_t Index = 1; Index < LeftCount; ++Index)
	{
		LeftBounds = UnionBounds(LeftBounds, ScratchItems[static_cast<size_t>(Index)].Bounds);
	}

	FEditorAxisAlignedBox RightBounds = ScratchItems[static_cast<size_t>(LeftCount)].Bounds;
	for (int32_t Index = LeftCount + 1; Index < InCount; ++Index)
	{
		RightBounds = UnionBounds(RightBounds, ScratchItems[static_cast<size_t>(Index)].Bounds);
	}

	const float ParentArea = std::max(ComputeAabbSurfaceArea(InParentBounds), 1e-5f);
	const float LeftArea = ComputeAabbSurfaceArea(LeftBounds);
	const float RightArea = ComputeAabbSurfaceArea(RightBounds);
	return kSplitCostTraversal
		+ (LeftArea / ParentArea) * static_cast<float>(LeftCount) * kSplitCostIntersect
		+ (RightArea / ParentArea) * static_cast<float>(InCount - LeftCount) * kSplitCostIntersect;
}

int32_t FEditorTriangleBvh::BuildRecursiveImpl(
	std::vector<FBuildItem>& InOutItems,
	int32_t InStart,
	int32_t InCount,
	EPickTriangleBvhSplitMethod InSplitMethod)
{
	const int32_t NodeIndex = static_cast<int32_t>(Nodes_.size());
	Nodes_.push_back(FNode{});

	if (InCount <= kMaxTrianglesPerLeaf)
	{
		FNode& LeafNode = Nodes_[static_cast<size_t>(NodeIndex)];
		LeafNode.bIsLeaf = true;
		LeafNode.Bounds = ComputeBoundsForItems(InOutItems, InStart, InCount);
		LeafNode.FirstTriangle = static_cast<int32_t>(TriangleIndices_.size());
		LeafNode.TriangleCount = InCount;
		for (int32_t Index = 0; Index < InCount; ++Index)
		{
			TriangleIndices_.push_back(InOutItems[static_cast<size_t>(InStart + Index)].TriangleIndex);
		}
		return NodeIndex;
	}

	const FEditorAxisAlignedBox NodeBounds = ComputeBoundsForItems(InOutItems, InStart, InCount);
	int32_t SplitAxis = ChooseLongestAxis(NodeBounds);
	int32_t LeftCount = InCount / 2;
	float SplitValue = GetCentroidAxisValue(
		InOutItems[static_cast<size_t>(InStart + LeftCount)].Centroid,
		SplitAxis);

	if (InSplitMethod == EPickTriangleBvhSplitMethod::Sah)
	{
		float BestCost = std::numeric_limits<float>::max();
		int32_t BestAxis = SplitAxis;
		float BestSplitValue = SplitValue;
		int32_t BestLeftCount = LeftCount;

		for (int32_t Axis = 0; Axis < 3; ++Axis)
		{
			const float AxisMin = GetBoundsAxisValue(NodeBounds, Axis, false);
			const float AxisMax = GetBoundsAxisValue(NodeBounds, Axis, true);
			const int32_t BucketCount = 12;
			for (int32_t BucketIndex = 1; BucketIndex < BucketCount; ++BucketIndex)
			{
				const float CandidateSplit = AxisMin
					+ (AxisMax - AxisMin) * (static_cast<float>(BucketIndex) / static_cast<float>(BucketCount));
				int32_t CandidateLeftCount = 0;
				std::vector<FBuildItem> ScratchItems(
					InOutItems.begin() + InStart,
					InOutItems.begin() + InStart + InCount);
				PartitionByAxis(ScratchItems, 0, InCount, Axis, CandidateSplit, &CandidateLeftCount);
				if (CandidateLeftCount <= 0 || CandidateLeftCount >= InCount)
				{
					continue;
				}

				const float CandidateCost = EvaluateSahSplitCost(
					InOutItems,
					InStart,
					InCount,
					Axis,
					CandidateSplit,
					NodeBounds);
				if (CandidateCost < BestCost)
				{
					BestCost = CandidateCost;
					BestAxis = Axis;
					BestSplitValue = CandidateSplit;
					BestLeftCount = CandidateLeftCount;
				}
			}
		}

		SplitAxis = BestAxis;
		SplitValue = BestSplitValue;
		LeftCount = BestLeftCount;
		PartitionByAxis(InOutItems, InStart, InCount, SplitAxis, SplitValue, &LeftCount);
	}
	else
	{
		PartitionByAxis(InOutItems, InStart, InCount, SplitAxis, SplitValue, &LeftCount);
	}

	const int32_t LeftChild = BuildRecursiveImpl(InOutItems, InStart, LeftCount, InSplitMethod);
	const int32_t RightChild = BuildRecursiveImpl(
		InOutItems,
		InStart + LeftCount,
		InCount - LeftCount,
		InSplitMethod);

	FNode& InternalNode = Nodes_[static_cast<size_t>(NodeIndex)];
	InternalNode.bIsLeaf = false;
	InternalNode.Bounds = NodeBounds;
	InternalNode.LeftChild = LeftChild;
	InternalNode.RightChild = RightChild;
	return NodeIndex;
}

void FEditorTriangleBvh::Query(
	const FWorldRay& InLocalRay,
	uint32_t InFirstIndex,
	uint32_t InIndexCount,
	float InMaxT,
	FEditorTriangleBvhQueryResult* OutResult) const
{
	if (OutResult == nullptr || Nodes_.empty() || LocalPositions_.empty() || MeshIndices_.size() < 3)
	{
		return;
	}

	OutResult->bHit = false;
	OutResult->HitT = InMaxT;

	std::vector<int32_t> NodeStack;
	NodeStack.push_back(0);
	const uint32_t MeshIndexCount = static_cast<uint32_t>(MeshIndices_.size());

	while (!NodeStack.empty())
	{
		const int32_t NodeIndex = NodeStack.back();
		NodeStack.pop_back();
		if (NodeIndex < 0 || NodeIndex >= static_cast<int32_t>(Nodes_.size()))
		{
			continue;
		}

		const FNode& Node = Nodes_[static_cast<size_t>(NodeIndex)];
		float EnterT = 0.0f;
		if (!FEditorRayIntersection::RayIntersectsAabb(InLocalRay, Node.Bounds, &EnterT, nullptr))
		{
			continue;
		}
		if (EnterT > OutResult->HitT)
		{
			continue;
		}

		if (Node.bIsLeaf)
		{
			for (int32_t TriangleOffset = 0; TriangleOffset < Node.TriangleCount; ++TriangleOffset)
			{
				const size_t TriangleIndicesIndex =
					static_cast<size_t>(Node.FirstTriangle + TriangleOffset);
				if (Node.FirstTriangle < 0
					|| TriangleIndicesIndex >= TriangleIndices_.size())
				{
					continue;
				}

				const int32_t StoredIndex = TriangleIndices_[TriangleIndicesIndex];
				if (!TriangleOverlapsIndexRange(StoredIndex, InFirstIndex, InIndexCount, MeshIndexCount))
				{
					continue;
				}

				const uint32_t I0 = MeshIndices_[static_cast<size_t>(StoredIndex) * 3 + 0];
				const uint32_t I1 = MeshIndices_[static_cast<size_t>(StoredIndex) * 3 + 1];
				const uint32_t I2 = MeshIndices_[static_cast<size_t>(StoredIndex) * 3 + 2];
				if (I0 >= LocalPositions_.size()
					|| I1 >= LocalPositions_.size()
					|| I2 >= LocalPositions_.size())
				{
					continue;
				}

				FRayTriangleHit TriangleHit{};
				if (!FEditorRayIntersection::RayIntersectsTriangle(
						InLocalRay,
						LocalPositions_[I0],
						LocalPositions_[I1],
						LocalPositions_[I2],
						OutResult->HitT,
						&TriangleHit))
				{
					continue;
				}

				OutResult->bHit = true;
				OutResult->HitT = TriangleHit.T;
			}
		}
		else
		{
			if (Node.RightChild >= 0)
			{
				NodeStack.push_back(Node.RightChild);
			}
			if (Node.LeftChild >= 0)
			{
				NodeStack.push_back(Node.LeftChild);
			}
		}
	}
}

const FEditorTriangleBvh* FEditorTriangleBvhCache::GetOrBuild(
	const UStaticMesh* InStaticMesh,
	EPickTriangleBvhSplitMethod InSplitMethod)
{
	if (InStaticMesh == nullptr)
	{
		return nullptr;
	}

	const size_t VertexCount = InStaticMesh->GetVertices().size();
	const size_t IndexCount = InStaticMesh->GetIndices().size();
	for (FCacheEntry& Entry : Entries_)
	{
		if (Entry.Mesh == InStaticMesh
			&& Entry.SplitMethod == InSplitMethod
			&& Entry.VertexCount == VertexCount
			&& Entry.IndexCount == IndexCount
			&& Entry.Bvh.IsBuilt())
		{
			return &Entry.Bvh;
		}
	}

	std::vector<FVector3> LocalPositions;
	LocalPositions.reserve(VertexCount);
	for (const UStaticMesh::FVertex& Vertex : InStaticMesh->GetVertices())
	{
		LocalPositions.push_back(Vertex.Position);
	}

	FCacheEntry NewEntry{};
	NewEntry.Mesh = InStaticMesh;
	NewEntry.SplitMethod = InSplitMethod;
	NewEntry.VertexCount = VertexCount;
	NewEntry.IndexCount = IndexCount;
	NewEntry.Bvh.Build(LocalPositions, InStaticMesh->GetIndices(), InSplitMethod);
	Entries_.push_back(std::move(NewEntry));
	return &Entries_.back().Bvh;
}

const FEditorTriangleBvh* FEditorTriangleBvhCache::GetOrBuild(
	const USkeletalMesh* InSkeletalMesh,
	EPickTriangleBvhSplitMethod InSplitMethod)
{
	if (InSkeletalMesh == nullptr)
	{
		return nullptr;
	}

	const size_t VertexCount = InSkeletalMesh->GetSkinVertices().size();
	const size_t IndexCount = InSkeletalMesh->GetIndices().size();
	for (FCacheEntry& Entry : Entries_)
	{
		if (Entry.Mesh == InSkeletalMesh
			&& Entry.SplitMethod == InSplitMethod
			&& Entry.VertexCount == VertexCount
			&& Entry.IndexCount == IndexCount
			&& Entry.Bvh.IsBuilt())
		{
			return &Entry.Bvh;
		}
	}

	std::vector<FVector3> LocalPositions;
	LocalPositions.reserve(VertexCount);
	for (const USkinnedAsset::FSkinVertex& Vertex : InSkeletalMesh->GetSkinVertices())
	{
		LocalPositions.push_back(Vertex.Position);
	}

	FCacheEntry NewEntry{};
	NewEntry.Mesh = InSkeletalMesh;
	NewEntry.SplitMethod = InSplitMethod;
	NewEntry.VertexCount = VertexCount;
	NewEntry.IndexCount = IndexCount;
	NewEntry.Bvh.Build(LocalPositions, InSkeletalMesh->GetIndices(), InSplitMethod);
	Entries_.push_back(std::move(NewEntry));
	return &Entries_.back().Bvh;
}

void FEditorTriangleBvhCache::InvalidateAll()
{
	Entries_.clear();
}
