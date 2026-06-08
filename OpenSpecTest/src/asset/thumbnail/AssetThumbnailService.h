#pragma once

#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <QImage>
#include <QObject>

#include "asset/AssetRegistry.h"

class IAssetThumbnailProvider;

class FAssetThumbnailService final : public QObject
{
	Q_OBJECT

public:
	static FAssetThumbnailService& Get();

	~FAssetThumbnailService() override;

	void InvalidateAll();
	void InvalidateEntry(const FAssetRegistryEntry& InEntry);

	bool TryGetCached(const FAssetRegistryEntry& InEntry, QImage* OutImage) const;
	void RequestThumbnail(const FAssetRegistryEntry& InEntry, int InSize, bool bHighPriority = false);

signals:
	void ThumbnailReady(const QString& InCacheKey, const QImage& InImage);

private slots:
	void ProcessQueueOnMainThread();

private:
	FAssetThumbnailService();

	QString BuildCacheKey(const FAssetRegistryEntry& InEntry) const;
	QImage GenerateThumbnail(const FAssetRegistryEntry& InEntry, int InSize) const;
	void EnqueueRequest(const FAssetRegistryEntry& InEntry, int InSize, bool bHighPriority);
	void ScheduleQueueProcessing();

	struct FPendingRequest
	{
		FAssetRegistryEntry Entry;
		int Size = 128;
	};

	std::vector<std::unique_ptr<IAssetThumbnailProvider>> Providers_;
	std::unique_ptr<IAssetThumbnailProvider> DefaultProvider_;
	std::unordered_map<std::string, QImage> Cache_;
	std::list<FPendingRequest> PendingQueue_;
	bool m_is_processing_ = false;
	static constexpr int kMaxThumbnailsPerTick = 2;
};
