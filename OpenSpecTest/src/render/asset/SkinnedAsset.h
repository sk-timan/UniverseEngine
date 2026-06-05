#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>

#include "core/UClass.h"
#include "render/asset/StreamableRenderAsset.h"

#include "math/FTransform.h"
#include "math/FVector3.h"
#include "math/FVector2D.h"
#include "math/FVector4.h"

class USkinnedAsset : public UStreamableRenderAsset
{
public:
	struct FBone
	{
		std::string Name;
		int32_t ParentIndex = -1;
		FTransform ReferencePose;
	};

	struct FSkinVertex
	{
		FVector3 Position;
		FVector3 Normal;
		FVector2D TexCoord;
		FVector4 Tangent;
		std::array<int32_t, 4> BoneIndices{};
		std::array<float, 4> BoneWeights{};
	};

	USkinnedAsset(uint64_t InObjectId, std::string InObjectName, const UClass* InClass = nullptr);
	virtual ~USkinnedAsset() = default;

	static const UClass& StaticClass();

	void SetSkeleton(const std::vector<FBone>& InBones);
	const std::vector<FBone>& GetSkeleton() const;
	int32_t FindBoneIndex(const std::string& InBoneName) const;

	void SetSkinVertices(const std::vector<FSkinVertex>& InVertices);
	void SetSkinVertices(const std::vector<FSkinVertex>& InVertices, bool bUpdateBounds);
	const std::vector<FSkinVertex>& GetSkinVertices() const;

	virtual bool HasResidentGeometryData() const override;
	virtual FBounds GetBounds() const override;

private:
	void RebuildBoundsFromSkinVertices();

	std::vector<FBone> Skeleton_;
	std::vector<FSkinVertex> SkinVertices_;
	FBounds TotalBounds_{};
};
