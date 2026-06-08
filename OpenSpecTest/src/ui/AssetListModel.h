#pragma once

#include <QAbstractListModel>
#include <QColor>
#include <QImage>
#include <vector>

#include "asset/AssetRegistry.h"

enum class EAssetListRole
{
	SoftObjectPath = Qt::UserRole,
	AssetType,
	UAssetFilePath,
	AccentColor,
	TypeDisplayName,
	CacheKey,
	ThumbnailImage,
};

struct FAssetBrowserListItem
{
	FAssetRegistryEntry Entry;
	QImage Thumbnail;
	bool bThumbnailPending = false;
};

class AssetListModel final : public QAbstractListModel
{
	Q_OBJECT

public:
	explicit AssetListModel(QObject* InParent = nullptr);

	void SetItems(std::vector<FAssetBrowserListItem> InItems);
	const FAssetBrowserListItem* GetItemAt(int InRow) const;
	void UpdateThumbnail(const QString& InCacheKey, const QImage& InImage);
	void MarkThumbnailPending(int InRow);
	void ClearThumbnailPending(const QString& InCacheKey);

	int rowCount(const QModelIndex& InParent = QModelIndex()) const override;
	QVariant data(const QModelIndex& InIndex, int InRole = Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex& InIndex) const override;
	QHash<int, QByteArray> roleNames() const override;

private:
	std::vector<FAssetBrowserListItem> Items_;
};
