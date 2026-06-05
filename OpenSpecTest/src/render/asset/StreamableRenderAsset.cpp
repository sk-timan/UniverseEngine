#include "render/asset/StreamableRenderAsset.h"

#include <utility>

#include "core/ObjectRegistry.h"

namespace
{
std::unique_ptr<UStreamableRenderAsset> CreateStreamableRenderAssetInstance(uint64_t InObjectId, std::string InObjectName)
{
	return std::make_unique<UStreamableRenderAsset>(InObjectId, std::move(InObjectName), &UStreamableRenderAsset::StaticClass());
}
} // namespace

UStreamableRenderAsset::UStreamableRenderAsset(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
	: UObject(InObjectId, std::move(InObjectName), InClass != nullptr ? InClass : &UStreamableRenderAsset::StaticClass())
{
}

const UClass& UStreamableRenderAsset::StaticClass()
{
	static const UClass Class("UStreamableRenderAsset", &UObject::StaticClass(), CreateStreamableRenderAssetInstance);
	return Class;
}

void UStreamableRenderAsset::SetAssetPath(const std::filesystem::path& InRelativePath)
{
	AssetPath_ = InRelativePath;
}

const std::filesystem::path& UStreamableRenderAsset::GetAssetPath() const
{
	return AssetPath_;
}

UStreamableRenderAsset::ELoadingStatus UStreamableRenderAsset::GetLoadingStatus() const
{
	return LoadingStatus_;
}

void UStreamableRenderAsset::RequestLoad()
{
	LoadingStatus_ = ELoadingStatus::Loaded;
}

void UStreamableRenderAsset::Release()
{
	LoadingStatus_ = ELoadingStatus::Unloaded;
}

bool UStreamableRenderAsset::HasResidentGeometryData() const
{
	return false;
}

UStreamableRenderAsset::FBounds UStreamableRenderAsset::GetBounds() const
{
	FBounds DefaultBounds;
	DefaultBounds.Origin = {0.0f, 0.0f, 0.0f};
	DefaultBounds.Extent = {0.0f, 0.0f, 0.0f};
	DefaultBounds.SphereRadius = 0.0f;
	return DefaultBounds;
}

void UStreamableRenderAsset::Serialize(nlohmann::json* OutObjectJson) const
{
	UObject::Serialize(OutObjectJson);
	if (OutObjectJson == nullptr)
	{
		return;
	}
	(*OutObjectJson)["asset_path"] = AssetPath_.string();
	(*OutObjectJson)["loading_status"] = static_cast<int>(LoadingStatus_);
}
