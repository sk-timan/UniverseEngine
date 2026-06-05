#pragma once

#include <filesystem>
#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

#include "core/UClass.h"
#include "core/UObject.h"
#include "math/FVector3.h"

class UStreamableRenderAsset : public UObject
{
public:
	UStreamableRenderAsset(uint64_t InObjectId, std::string InObjectName, const UClass* InClass = nullptr);
	virtual ~UStreamableRenderAsset() = default;

	static const UClass& StaticClass();

	void SetAssetPath(const std::filesystem::path& InRelativePath);
	const std::filesystem::path& GetAssetPath() const;

	enum class ELoadingStatus
	{
		Unloaded,
		Loading,
		Loaded,
		Failed
	};

	ELoadingStatus GetLoadingStatus() const;
	virtual void RequestLoad();
	virtual void Release();
	virtual bool HasResidentGeometryData() const;

	struct FBounds
	{
		FVector3 Origin;
		FVector3 Extent;
		float SphereRadius;
	};

	virtual FBounds GetBounds() const;

	virtual void Serialize(nlohmann::json* OutObjectJson) const override;

protected:
	std::filesystem::path AssetPath_;
	ELoadingStatus LoadingStatus_ = ELoadingStatus::Loaded;
};
