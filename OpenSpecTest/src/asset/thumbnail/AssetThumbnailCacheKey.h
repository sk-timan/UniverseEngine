#pragma once

#include <QString>

struct FAssetRegistryEntry;

QString BuildAssetThumbnailCacheKey(const FAssetRegistryEntry& InEntry);
