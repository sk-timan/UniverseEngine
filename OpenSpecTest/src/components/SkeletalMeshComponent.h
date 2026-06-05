#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/UClass.h"
#include "core/UObject.h"
#include "components/SkinnedMeshComponent.h"

class USkeletalMesh;

class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
	USkeletalMeshComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass = nullptr);
	virtual ~USkeletalMeshComponent() = default;

	static const UClass& StaticClass();

	void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh);
	USkeletalMesh* GetSkeletalMesh() const;

	virtual void CreateRenderState(FPrimitiveRenderState* OutRenderState) override;
	virtual void UpdateRenderState(FPrimitiveRenderState* InOutRenderState) override;

private:
	USkeletalMesh* SkeletalMesh_ = nullptr;
};
