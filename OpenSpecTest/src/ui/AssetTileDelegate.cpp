#include "ui/AssetTileDelegate.h"

#include <QAbstractItemView>
#include <QPainter>

#include "ui/AssetBrowserPanelWidget.h"
#include "ui/AssetListModel.h"

namespace
{
QWidget* ResolveAssetTilePropertyHost(const QStyleOptionViewItem& InOption)
{
	if (InOption.widget == nullptr)
	{
		return nullptr;
	}

	if (const auto* ItemView = qobject_cast<const QAbstractItemView*>(InOption.widget))
	{
		return ItemView->viewport();
	}

	return const_cast<QWidget*>(InOption.widget);
}

int ReadAssetTileRowProperty(const QStyleOptionViewItem& InOption, const char* InPropertyName)
{
	auto TryRead = [InPropertyName](QWidget* InWidget) -> int
	{
		if (InWidget == nullptr)
		{
			return kAssetDragSourceNone;
		}

		const QVariant Value = InWidget->property(InPropertyName);
		return Value.isValid() ? Value.toInt() : kAssetDragSourceNone;
	};

	int RowValue = TryRead(ResolveAssetTilePropertyHost(InOption));
	if (RowValue != kAssetDragSourceNone)
	{
		return RowValue;
	}

	RowValue = TryRead(const_cast<QWidget*>(InOption.widget));
	if (RowValue != kAssetDragSourceNone)
	{
		return RowValue;
	}

	if (InOption.widget != nullptr && InOption.widget->parentWidget() != nullptr)
	{
		RowValue = TryRead(InOption.widget->parentWidget());
	}

	return RowValue;
}
} // namespace

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
	const int DragSourceRow = ReadAssetTileRowProperty(InOption, "assetDragSourceRow");
	const bool bDragSource =
		DragSourceRow >= 0 && InIndex.isValid() && InIndex.row() == DragSourceRow;
	const int DropHoverRow = ReadAssetTileRowProperty(InOption, "folderDropHoverRow");
	const bool bIsFolder = InIndex.data(static_cast<int>(EAssetListRole::IsFolder)).toBool();
	const bool bDropHover =
		bIsFolder && DropHoverRow >= 0 && InIndex.isValid() && InIndex.row() == DropHoverRow;
	const int ItemHoverRow = ReadAssetTileRowProperty(InOption, "itemHoverRow");
	const bool bItemHover =
		!bDragSource
		&& ItemHoverRow >= 0
		&& InIndex.isValid()
		&& InIndex.row() == ItemHoverRow;
	const bool bSelected = (InOption.state & QStyle::State_Selected) || bDragSource;

	InPainter->save();
	InPainter->setRenderHint(QPainter::Antialiasing, true);

	QColor TileBackgroundColor("#2d2d30");
	if (bSelected)
	{
		TileBackgroundColor = QColor("#2c5d87");
	}
	else if (bDropHover)
	{
		TileBackgroundColor = QColor("#2a2d2e");
	}
	else if (bItemHover)
	{
		TileBackgroundColor = QColor("#3a3d45");
	}

	InPainter->fillRect(TileRect, TileBackgroundColor);
	if (bDragSource || bDropHover)
	{
		InPainter->setPen(QPen(QColor("#4da3ff"), 2));
		InPainter->setBrush(Qt::NoBrush);
		InPainter->drawRoundedRect(TileRect.adjusted(1, 1, -1, -1), 3, 3);
	}
	else if (bItemHover)
	{
		InPainter->setPen(QPen(QColor("#6a9fd8"), 2));
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
	if (bIsFolder)
	{
		const int FolderWidth = ThumbnailRect.width() * 3 / 4;
		const int FolderHeight = ThumbnailRect.height() * 3 / 5;
		const QRect FolderBodyRect(
			ThumbnailRect.center().x() - FolderWidth / 2,
			ThumbnailRect.center().y() - FolderHeight / 4,
			FolderWidth,
			FolderHeight);
		const QRect FolderTabRect(
			FolderBodyRect.left(),
			FolderBodyRect.top() - FolderHeight / 5,
			FolderWidth / 2,
			FolderHeight / 5);

		InPainter->setPen(Qt::NoPen);
		InPainter->setBrush(QColor("#c9972e"));
		InPainter->drawRoundedRect(FolderTabRect, 2, 2);
		InPainter->setBrush(QColor("#e0b84d"));
		InPainter->drawRoundedRect(FolderBodyRect, 3, 3);
	}
	else if (!Thumbnail.isNull())
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
