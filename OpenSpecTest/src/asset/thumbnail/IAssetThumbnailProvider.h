#pragma once

#include <QImage>

#include "asset/AssetRegistry.h"

class IAssetThumbnailProvider
{
public:
	virtual ~IAssetThumbnailProvider() = default;
	virtual bool CanProvide(const std::string& InType) const = 0;
	virtual QImage Generate(const FAssetRegistryEntry& InEntry, int InSize) const = 0;
};
