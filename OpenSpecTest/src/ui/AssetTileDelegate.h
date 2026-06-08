#pragma once

#include <QStyledItemDelegate>

class AssetTileDelegate final : public QStyledItemDelegate
{
	Q_OBJECT

public:
	explicit AssetTileDelegate(QObject* InParent = nullptr);

	QSize sizeHint(const QStyleOptionViewItem& InOption, const QModelIndex& InIndex) const override;
	void paint(QPainter* InPainter, const QStyleOptionViewItem& InOption, const QModelIndex& InIndex) const override;

private:
	static constexpr int kTileWidth = 100;
	static constexpr int kTileHeight = 130;
	static constexpr int kThumbnailSize = 80;
};
