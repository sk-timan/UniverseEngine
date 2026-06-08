#include "asset/thumbnail/AssetThumbnailCacheKey.h"

#include <filesystem>

#include <QString>

#include "asset/AssetRegistry.h"

QString BuildAssetThumbnailCacheKey(const FAssetRegistryEntry& InEntry)
{
	std::error_code ErrorCode;
	const auto FileTime = std::filesystem::last_write_time(InEntry.UAssetFilePath, ErrorCode);
	const std::string GuidPart = InEntry.Guid.empty() ? InEntry.AssetPath : InEntry.Guid;
	const std::string CacheKey = GuidPart + ":" + std::to_string(FileTime.time_since_epoch().count());
	return QString::fromStdString(CacheKey);
}
