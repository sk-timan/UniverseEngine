#include "asset/thumbnail/AssetThumbnailService.h"

#include "asset/thumbnail/AssetThumbnailCacheKey.h"

#include <QTimer>

#include "asset/AssetTypeInfo.h"
#include "asset/thumbnail/DefaultThumbnailProvider.h"
#include "asset/thumbnail/IAssetThumbnailProvider.h"
#include "asset/thumbnail/MeshThumbnailProvider.h"
#include "asset/thumbnail/TextureThumbnailProvider.h"

FAssetThumbnailService& FAssetThumbnailService::Get()
{
	static FAssetThumbnailService Instance;
	return Instance;
}

FAssetThumbnailService::FAssetThumbnailService()
{
	Providers_.push_back(std::make_unique<MeshThumbnailProvider>());
	Providers_.push_back(std::make_unique<TextureThumbnailProvider>());
	DefaultProvider_ = std::make_unique<DefaultThumbnailProvider>();
}

FAssetThumbnailService::~FAssetThumbnailService() = default;

void FAssetThumbnailService::InvalidateAll()
{
	PendingQueue_.clear();
	Cache_.clear();
	m_is_processing_ = false;
}

void FAssetThumbnailService::InvalidateEntry(const FAssetRegistryEntry& InEntry)
{
	Cache_.erase(BuildCacheKey(InEntry).toStdString());
}

bool FAssetThumbnailService::TryGetCached(const FAssetRegistryEntry& InEntry, QImage* OutImage) const
{
	if (OutImage == nullptr)
	{
		return false;
	}

	const auto Found = Cache_.find(BuildCacheKey(InEntry).toStdString());
	if (Found == Cache_.end())
	{
		return false;
	}

	*OutImage = Found->second;
	return true;
}

void FAssetThumbnailService::RequestThumbnail(
	const FAssetRegistryEntry& InEntry,
	int InSize,
	bool bHighPriority)
{
	QImage CachedImage;
	if (TryGetCached(InEntry, &CachedImage))
	{
		emit ThumbnailReady(BuildCacheKey(InEntry), CachedImage);
		return;
	}

	EnqueueRequest(InEntry, InSize, bHighPriority);
	ScheduleQueueProcessing();
}

QString FAssetThumbnailService::BuildCacheKey(const FAssetRegistryEntry& InEntry) const
{
	return BuildAssetThumbnailCacheKey(InEntry);
}

QImage FAssetThumbnailService::GenerateThumbnail(const FAssetRegistryEntry& InEntry, int InSize) const
{
	for (const std::unique_ptr<IAssetThumbnailProvider>& Provider : Providers_)
	{
		if (Provider != nullptr && Provider->CanProvide(InEntry.Type))
		{
			QImage Image = Provider->Generate(InEntry, InSize);
			if (!Image.isNull())
			{
				return Image;
			}
		}
	}

	if (DefaultProvider_ != nullptr)
	{
		return DefaultProvider_->Generate(InEntry, InSize);
	}
	return QImage();
}

void FAssetThumbnailService::EnqueueRequest(
	const FAssetRegistryEntry& InEntry,
	int InSize,
	bool bHighPriority)
{
	const QString CacheKey = BuildCacheKey(InEntry);
	if (Cache_.contains(CacheKey.toStdString()))
	{
		return;
	}

	for (const FPendingRequest& Pending : PendingQueue_)
	{
		if (BuildCacheKey(Pending.Entry) == CacheKey)
		{
			return;
		}
	}

	FPendingRequest Request;
	Request.Entry = InEntry;
	Request.Size = InSize;
	if (bHighPriority)
	{
		PendingQueue_.push_front(std::move(Request));
	}
	else
	{
		PendingQueue_.push_back(std::move(Request));
	}
}

void FAssetThumbnailService::ScheduleQueueProcessing()
{
	if (m_is_processing_)
	{
		return;
	}

	m_is_processing_ = true;
	QTimer::singleShot(0, this, &FAssetThumbnailService::ProcessQueueOnMainThread);
}

void FAssetThumbnailService::ProcessQueueOnMainThread()
{
	int ProcessedCount = 0;
	while (!PendingQueue_.empty() && ProcessedCount < kMaxThumbnailsPerTick)
	{
		const FPendingRequest Request = PendingQueue_.front();
		PendingQueue_.pop_front();

		const QString CacheKey = BuildCacheKey(Request.Entry);
		QImage Image = GenerateThumbnail(Request.Entry, Request.Size);
		if (Image.isNull() && DefaultProvider_ != nullptr)
		{
			Image = DefaultProvider_->Generate(Request.Entry, Request.Size);
		}
		if (!Image.isNull())
		{
			Cache_[CacheKey.toStdString()] = Image;
			emit ThumbnailReady(CacheKey, Image);
		}

		++ProcessedCount;
	}

	if (!PendingQueue_.empty())
	{
		QTimer::singleShot(0, this, &FAssetThumbnailService::ProcessQueueOnMainThread);
		return;
	}

	m_is_processing_ = false;
}
