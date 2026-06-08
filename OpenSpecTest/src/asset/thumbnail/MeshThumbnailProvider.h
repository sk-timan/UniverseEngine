#pragma once

#include <QImage>

#include "asset/AssetRegistry.h"
#include "asset/thumbnail/IAssetThumbnailProvider.h"

class MeshThumbnailProvider final : public IAssetThumbnailProvider
{
public:
	bool CanProvide(const std::string& InType) const override;
	QImage Generate(const FAssetRegistryEntry& InEntry, int InSize) const override;
};
