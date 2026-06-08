#include "ui/AssetTileDelegate.h"

#include <QPainter>

#include "ui/AssetBrowserPanelWidget.h"
#include "ui/AssetListModel.h"

AssetTileDelegate::AssetTileDelegate(QObject* InParent)
	: QStyledItemDelegate(InParent)
{
}

QSize AssetTileDelegate::sizeHint(const QStyleOptionViewItem& InOption, const QModelIndex& InIndex) const
{
	(void)InOption;
	(void)InIndex;
	return {kTileWidth, kTileHeight};
}

void AssetTileDelegate::paint(
	QPainter* InPainter,
	const QStyleOptionViewItem& InOption,
	const QModelIndex& InIndex) const
{
	if (!InIndex.isValid() || InPainter == nullptr)
	{
		return;
	}

	const QRect TileRect = InOption.rect.adjusted(2, 2, -2, -2);
	int DragSourceRow = kAssetDragSourceNone;
	if (InOption.widget != nullptr)
	{
		const QVariant DragRowProp = InOption.widget->property("assetDragSourceRow");
		if (DragRowProp.isValid())
		{
			DragSourceRow = DragRowProp.toInt();
		}
	}
	const bool bDragSource =
		DragSourceRow >= 0 && InIndex.isValid() && InIndex.row() == DragSourceRow;
	const bool bSelected = (InOption.state & QStyle::State_Selected) || bDragSource;

	InPainter->save();
	InPainter->setRenderHint(QPainter::Antialiasing, true);

	InPainter->fillRect(TileRect, bSelected ? QColor("#2c5d87") : QColor("#2d2d30"));
	if (bDragSource)
	{
		InPainter->setPen(QPen(QColor("#4da3ff"), 2));
		InPainter->setBrush(Qt::NoBrush);
		InPainter->drawRoundedRect(TileRect.adjusted(1, 1, -1, -1), 3, 3);
	}

	const QRect ThumbnailRect(
		TileRect.left() + (TileRect.width() - kThumbnailSize) / 2,
		TileRect.top() + 4,
		kThumbnailSize,
		kThumbnailSize);
	InPainter->fillRect(ThumbnailRect, QColor("#1a1a1e"));

	const QImage Thumbnail = InIndex.data(static_cast<int>(EAssetListRole::ThumbnailImage)).value<QImage>();
	if (!Thumbnail.isNull())
	{
		InPainter->drawImage(ThumbnailRect, Thumbnail);
	}
	else
	{
		InPainter->setPen(QPen(QColor("#4a4a50"), 1, Qt::DashLine));
		InPainter->drawRect(ThumbnailRect.adjusted(1, 1, -1, -1));
	}

	const QColor AccentColor = InIndex.data(static_cast<int>(EAssetListRole::AccentColor)).value<QColor>();
	const int StripY = ThumbnailRect.bottom() + 2;
	InPainter->fillRect(TileRect.left() + 4, StripY, TileRect.width() - 8, 2, AccentColor);

	const QString ObjectName = InIndex.data(Qt::DisplayRole).toString();
	const QString TypeName = InIndex.data(static_cast<int>(EAssetListRole::TypeDisplayName)).toString();

	const QRect NameRect(TileRect.left() + 4, StripY + 6, TileRect.width() - 8, 16);
	const QRect TypeRect(TileRect.left() + 4, NameRect.bottom(), TileRect.width() - 8, 14);

	InPainter->setPen(QColor("#f0f0f0"));
	QFont NameFont = InPainter->font();
	NameFont.setPointSize(9);
	InPainter->setFont(NameFont);
	InPainter->drawText(NameRect, Qt::AlignHCenter | Qt::AlignVCenter, InPainter->fontMetrics().elidedText(ObjectName, Qt::ElideRight, NameRect.width()));

	InPainter->setPen(QColor("#9a9a9a"));
	QFont TypeFont = NameFont;
	TypeFont.setPointSize(8);
	InPainter->setFont(TypeFont);
	InPainter->drawText(TypeRect, Qt::AlignHCenter | Qt::AlignVCenter, TypeName);

	InPainter->restore();
}
