#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/UClass.h"
#include "core/UObject.h"
#include "components/MeshComponent.h"

#include "math/FTransform.h"

class USkinnedMeshComponent : public UMeshComponent
{
public:
	struct FBoneTransform
	{
		int32_t BoneIndex;
		FTransform Transform;
	};

	USkinnedMeshComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass = nullptr);
	virtual ~USkinnedMeshComponent() = default;

	static const UClass& StaticClass();

	void SetBoneTransforms(const std::vector<FBoneTransform>& InBoneTransforms);
	const std::vector<FBoneTransform>& GetBoneTransforms() const;

protected:
	std::vector<FBoneTransform> BoneTransforms_;
};
