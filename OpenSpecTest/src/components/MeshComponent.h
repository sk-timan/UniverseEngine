#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/UClass.h"
#include "core/UObject.h"
#include "components/PrimitiveComponent.h"

class UStreamableRenderAsset;

class UMeshComponent : public UPrimitiveComponent
{
public:
	UMeshComponent(uint64_t InObjectId, std::string InObjectName, const UClass* InClass = nullptr);
	virtual ~UMeshComponent() = default;

	static const UClass& StaticClass();

	void SetMeshAssetId(const std::string& InAssetId);
	const std::string& GetMeshAssetId() const;

	void SetMeshAsset(UStreamableRenderAsset* InAsset);
	UStreamableRenderAsset* GetMeshAsset() const;

	struct FMaterialOverride
	{
		int32_t MaterialSlot = 0;
		std::string MaterialAssetId;
	};

	void SetMaterialOverride(int32_t InSlot, const std::string& InMaterialAssetId);
	void ClearMaterialOverride(int32_t InSlot);
	const std::vector<FMaterialOverride>& GetMaterialOverrides() const;

	void SetForcedLODLevel(int32_t InLODLevel);
	int32_t GetForcedLODLevel() const;
	virtual int32_t GetCurrentLODLevel() const;

	virtual FBounds GetBounds() const override;

private:
	std::string MeshAssetId_;
	UStreamableRenderAsset* MeshAsset_ = nullptr;

	std::vector<FMaterialOverride> MaterialOverrides_;
	int32_t ForcedLODLevel_ = 0;
};
