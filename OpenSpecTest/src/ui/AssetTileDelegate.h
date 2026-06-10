#pragma once

#include <QSize>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>

class AssetTileDelegate final : public QStyledItemDelegate
{
	Q_OBJECT

public:
	explicit AssetTileDelegate(QObject* InParent = nullptr);

	QSize sizeHint(const QStyleOptionViewItem& InOption, const QModelIndex& InIndex) const override;
	void paint(QPainter* InPainter, const QStyleOptionViewItem& InOption, const QModelIndex& InIndex) const override;
};

inline constexpr int kMinAssetTileThumbnailSize = 48;
inline constexpr int kMaxAssetTileThumbnailSize = 160;
inline constexpr int kDefaultAssetTileThumbnailSize = 80;
inline constexpr int kAssetTileThumbnailStep = 8;
inline constexpr int kAssetTileHorizontalPadding = 20;
inline constexpr int kAssetTileTextAreaHeight = 50;

struct FAssetTileMetrics
{
	int ThumbnailSize = kDefaultAssetTileThumbnailSize;
	int TileWidth = kDefaultAssetTileThumbnailSize + kAssetTileHorizontalPadding;
	int TileHeight = kDefaultAssetTileThumbnailSize + kAssetTileTextAreaHeight;
};

FAssetTileMetrics BuildAssetTileMetrics(int InThumbnailSize);
int ReadAssetTileThumbnailSize(const QStyleOptionViewItem& InOption);
QSize BuildAssetTileSize(int InThumbnailSize);
