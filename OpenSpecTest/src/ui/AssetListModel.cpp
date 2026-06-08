#include "ui/AssetListModel.h"

#include "asset/AssetTypeInfo.h"
#include "asset/SoftObjectPath.h"
#include "asset/thumbnail/AssetThumbnailCacheKey.h"

AssetListModel::AssetListModel(QObject* InParent)
	: QAbstractListModel(InParent)
{
}

void AssetListModel::SetItems(std::vector<FAssetBrowserListItem> InItems)
{
	beginResetModel();
	Items_ = std::move(InItems);
	endResetModel();
}

const FAssetBrowserListItem* AssetListModel::GetItemAt(int InRow) const
{
	if (InRow < 0 || InRow >= static_cast<int>(Items_.size()))
	{
		return nullptr;
	}
	return &Items_[static_cast<size_t>(InRow)];
}

void AssetListModel::UpdateThumbnail(const QString& InCacheKey, const QImage& InImage)
{
	for (int Row = 0; Row < static_cast<int>(Items_.size()); ++Row)
	{
		const QString CacheKey = BuildAssetThumbnailCacheKey(Items_[static_cast<size_t>(Row)].Entry);
		if (CacheKey != InCacheKey)
		{
			continue;
		}

		Items_[static_cast<size_t>(Row)].Thumbnail = InImage;
		Items_[static_cast<size_t>(Row)].bThumbnailPending = false;
		const QModelIndex ChangedIndex = index(Row);
		emit dataChanged(ChangedIndex, ChangedIndex, {static_cast<int>(EAssetListRole::ThumbnailImage)});
		return;
	}
}

void AssetListModel::ClearThumbnailPending(const QString& InCacheKey)
{
	for (size_t Index = 0; Index < Items_.size(); ++Index)
	{
		const QString CacheKey = BuildAssetThumbnailCacheKey(Items_[Index].Entry);
		if (CacheKey != InCacheKey)
		{
			continue;
		}

		Items_[Index].bThumbnailPending = false;
		return;
	}
}

void AssetListModel::MarkThumbnailPending(int InRow)
{
	if (InRow < 0 || InRow >= static_cast<int>(Items_.size()))
	{
		return;
	}
	Items_[static_cast<size_t>(InRow)].bThumbnailPending = true;
}

int AssetListModel::rowCount(const QModelIndex& InParent) const
{
	if (InParent.isValid())
	{
		return 0;
	}
	return static_cast<int>(Items_.size());
}

Qt::ItemFlags AssetListModel::flags(const QModelIndex& InIndex) const
{
	if (!InIndex.isValid())
	{
		return Qt::NoItemFlags;
	}

	Qt::ItemFlags ItemFlags = QAbstractListModel::flags(InIndex);
	const FAssetBrowserListItem* Item = GetItemAt(InIndex.row());
	if (Item != nullptr && AssetTypeInfo::IsMeshAssetType(Item->Entry.Type))
	{
		ItemFlags |= Qt::ItemIsDragEnabled;
	}
	return ItemFlags;
}

QVariant AssetListModel::data(const QModelIndex& InIndex, int InRole) const
{
	if (!InIndex.isValid() || InIndex.row() < 0 || InIndex.row() >= static_cast<int>(Items_.size()))
	{
		return {};
	}

	const FAssetBrowserListItem& Item = Items_[static_cast<size_t>(InIndex.row())];
	const FAssetRegistryEntry& Entry = Item.Entry;
	const FAssetTypeDisplayInfo TypeInfo = AssetTypeInfo::GetDisplayInfo(Entry.Type);

	if (InRole == Qt::DisplayRole)
	{
		return QString::fromStdString(Entry.ObjectName);
	}

	switch (static_cast<EAssetListRole>(InRole))
	{
	case EAssetListRole::SoftObjectPath:
		return QString::fromStdString(FSoftObjectPath::Build(Entry.AssetPath, Entry.ObjectName));
	case EAssetListRole::AssetType:
		return QString::fromStdString(Entry.Type);
	case EAssetListRole::UAssetFilePath:
		return QString::fromStdString(Entry.UAssetFilePath.string());
	case EAssetListRole::AccentColor:
		return TypeInfo.AccentColor;
	case EAssetListRole::TypeDisplayName:
		return QString::fromStdString(TypeInfo.DisplayName);
	case EAssetListRole::CacheKey:
		return BuildAssetThumbnailCacheKey(Entry);
	case EAssetListRole::ThumbnailImage:
		return Item.Thumbnail;
	default:
		return {};
	}
}

QHash<int, QByteArray> AssetListModel::roleNames() const
{
	return {
		{static_cast<int>(EAssetListRole::SoftObjectPath), "softObjectPath"},
		{static_cast<int>(EAssetListRole::AssetType), "assetType"},
		{static_cast<int>(EAssetListRole::UAssetFilePath), "uassetFilePath"},
		{static_cast<int>(EAssetListRole::AccentColor), "accentColor"},
		{static_cast<int>(EAssetListRole::TypeDisplayName), "typeDisplayName"},
		{static_cast<int>(EAssetListRole::CacheKey), "cacheKey"},
		{static_cast<int>(EAssetListRole::ThumbnailImage), "thumbnailImage"},
	};
}
