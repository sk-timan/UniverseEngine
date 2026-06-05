#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/UClass.h"
#include "core/UObject.h"
#include "components/MeshComponent.h"

class UStaticMesh;

class UStaticMeshComponent : public UMeshComponent
{
public:
	UStaticMeshComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass = nullptr);
	virtual ~UStaticMeshComponent() = default;

	static const UClass& StaticClass();

	void SetStaticMesh(UStaticMesh* InStaticMesh);
	UStaticMesh* GetStaticMesh() const;

	virtual void CreateRenderState(FPrimitiveRenderState* OutRenderState) override;
	virtual void UpdateRenderState(FPrimitiveRenderState* InOutRenderState) override;
	virtual void DestroyRenderState(FPrimitiveRenderState* InRenderState) override;

private:
	UStaticMesh* StaticMesh_ = nullptr;
};
