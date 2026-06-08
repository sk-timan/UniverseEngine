#include "ui/AssetBrowserPanelWidget.h"

#include <algorithm>

#include <QApplication>
#include <QAbstractItemView>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QCursor>
#include <QDrag>
#include <QFileInfo>
#include <QPainter>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QScrollBar>
#include <QSplitter>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "app/GameApp.h"
#include "asset/AssetFolderTree.h"
#include "asset/AssetRegistry.h"
#include "asset/AssetTypeInfo.h"
#include "asset/MeshImportFactory.h"
#include "asset/ProjectPaths.h"
#include "asset/SoftObjectPath.h"
#include "asset/thumbnail/AssetThumbnailService.h"
#include "ui/AssetListModel.h"
#include "ui/AssetTileDelegate.h"

namespace
{
constexpr int kThumbnailSize = 128;

class AssetBrowserListView final : public QListView
{
public:
	using QListView::QListView;

protected:
	void startDrag(Qt::DropActions InSupportedActions) override
	{
		const QModelIndex Current = currentIndex();
		if (!Current.isValid())
		{
			return;
		}

		const QString SoftPath = Current.data(static_cast<int>(EAssetListRole::SoftObjectPath)).toString();
		const QString AssetType = Current.data(static_cast<int>(EAssetListRole::AssetType)).toString();
		if (!AssetTypeInfo::IsMeshAssetType(AssetType.toStdString()))
		{
			return;
		}

		auto* MimeData = new QMimeData();
		MimeData->setData(kSoftObjectPathMimeType, SoftPath.toUtf8());
		MimeData->setText(SoftPath);

		const QImage Thumbnail =
			Current.data(static_cast<int>(EAssetListRole::ThumbnailImage)).value<QImage>();
		constexpr int kDragPixmapSize = 48;
		QPixmap DragPixmap(kDragPixmapSize, kDragPixmapSize);
		DragPixmap.fill(Qt::transparent);
		{
			QPainter PixmapPainter(&DragPixmap);
			PixmapPainter.setRenderHint(QPainter::Antialiasing, true);
			PixmapPainter.setPen(QPen(QColor("#4da3ff"), 2));
			PixmapPainter.setBrush(QColor("#2c5d87"));
			PixmapPainter.drawRoundedRect(DragPixmap.rect().adjusted(1, 1, -1, -1), 4, 4);
			const QRect ImageRect = DragPixmap.rect().adjusted(4, 4, -4, -4);
			if (!Thumbnail.isNull())
			{
				PixmapPainter.drawImage(
					ImageRect,
					Thumbnail.scaled(ImageRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
			}
			else
			{
				PixmapPainter.setPen(QColor("#f0f0f0"));
				PixmapPainter.drawText(ImageRect, Qt::AlignCenter, tr("Mesh"));
			}
		}

		auto* Drag = new QDrag(this);
		Drag->setMimeData(MimeData);
		Drag->setPixmap(DragPixmap);
		Drag->setHotSpot(QPoint(kDragPixmapSize / 2, kDragPixmapSize / 2));

		viewport()->setProperty("assetDragSourceRow", Current.row());
		viewport()->update();

		const Qt::DropAction Result = Drag->exec(InSupportedActions, Qt::CopyAction);

		viewport()->setProperty("assetDragSourceRow", kAssetDragSourceNone);
		viewport()->update();

		if (Result == Qt::IgnoreAction && Current.isValid())
		{
			setCurrentIndex(Current);
		}
	}
};

void PopulateTreeItem(QTreeWidgetItem* InParentItem, const FAssetFolderNode& InNode)
{
	for (const std::unique_ptr<FAssetFolderNode>& Child : InNode.Children)
	{
		auto* Item = new QTreeWidgetItem(InParentItem, {QString::fromStdString(Child->DisplayName)});
		Item->setData(0, Qt::UserRole, QString::fromStdString(Child->Path));
		PopulateTreeItem(Item, *Child);
	}
}
} // namespace

AssetBrowserPanelWidget::AssetBrowserPanelWidget(GameApp* InGameApp, QWidget* InParent)
	: QWidget(InParent)
	, m_game_app_(InGameApp)
{
	BuildUi();
	connect(
		&FAssetThumbnailService::Get(),
		&FAssetThumbnailService::ThumbnailReady,
		this,
		&AssetBrowserPanelWidget::OnThumbnailReady);
	RefreshFromRegistry();
}

void AssetBrowserPanelWidget::BuildUi()
{
	auto* RootLayout = new QVBoxLayout(this);
	RootLayout->setContentsMargins(6, 6, 6, 6);
	RootLayout->setSpacing(6);

	auto* ToolbarLayout = new QHBoxLayout();
	m_search_edit_ = new QLineEdit(this);
	m_search_edit_->setObjectName("AssetBrowserSearchEdit");
	m_search_edit_->setPlaceholderText(tr("搜索资产..."));
	m_search_edit_->setClearButtonEnabled(true);
	ToolbarLayout->addWidget(m_search_edit_, 1);

	m_type_filter_combo_ = new QComboBox(this);
	m_type_filter_combo_->addItem(tr("全部类型"), QString());
	m_type_filter_combo_->addItem(tr("Static Mesh"), QStringLiteral("StaticMesh"));
	m_type_filter_combo_->addItem(tr("Skeletal Mesh"), QStringLiteral("SkeletalMesh"));
	m_type_filter_combo_->addItem(tr("其它"), QStringLiteral("__other__"));
	ToolbarLayout->addWidget(m_type_filter_combo_);
	RootLayout->addLayout(ToolbarLayout);

	m_splitter_ = new QSplitter(Qt::Horizontal, this);
	m_folder_tree_ = new QTreeWidget(m_splitter_);
	m_folder_tree_->setHeaderHidden(true);
	m_folder_tree_->setMinimumWidth(180);
	m_folder_tree_->setMaximumWidth(320);
	m_folder_tree_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_folder_tree_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	m_asset_grid_ = new AssetBrowserListView(m_splitter_);
	m_asset_grid_->setObjectName("AssetBrowserGrid");
	m_asset_grid_->setViewMode(QListView::IconMode);
	m_asset_grid_->setResizeMode(QListView::Adjust);
	m_asset_grid_->setMovement(QListView::Static);
	m_asset_grid_->setSpacing(8);
	m_asset_grid_->setSelectionMode(QAbstractItemView::SingleSelection);
	m_asset_grid_->setFocusPolicy(Qt::ClickFocus);
	m_asset_grid_->setDragEnabled(true);
	m_asset_grid_->setDragDropMode(QAbstractItemView::DragOnly);
	m_asset_grid_->setDefaultDropAction(Qt::CopyAction);
	m_asset_grid_->setContextMenuPolicy(Qt::CustomContextMenu);
	m_asset_grid_->setUniformItemSizes(true);
	m_asset_grid_->setWrapping(true);
	m_asset_grid_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_asset_grid_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	m_list_model_ = new AssetListModel(this);
	m_asset_grid_->setModel(m_list_model_);
	m_asset_grid_->setItemDelegate(new AssetTileDelegate(m_asset_grid_));
	m_asset_grid_->viewport()->setProperty("assetDragSourceRow", kAssetDragSourceNone);

	m_splitter_->addWidget(m_folder_tree_);
	m_splitter_->addWidget(m_asset_grid_);
	m_splitter_->setStretchFactor(0, 1);
	m_splitter_->setStretchFactor(1, 1);
	m_splitter_->setSizes({220, 600});
	RootLayout->addWidget(m_splitter_, 1);

	m_status_label_ = new QLabel(tr("0 items"), this);
	m_status_label_->setObjectName("SecondaryLabel");
	RootLayout->addWidget(m_status_label_);

	connect(m_search_edit_, &QLineEdit::textChanged, this, &AssetBrowserPanelWidget::OnSearchTextChanged);
	connect(
		m_type_filter_combo_,
		QOverload<int>::of(&QComboBox::currentIndexChanged),
		this,
		&AssetBrowserPanelWidget::OnTypeFilterChanged);
	connect(
		m_folder_tree_,
		&QTreeWidget::itemSelectionChanged,
		this,
		&AssetBrowserPanelWidget::OnFolderSelectionChanged);
	connect(
		m_asset_grid_,
		&QListView::customContextMenuRequested,
		this,
		&AssetBrowserPanelWidget::OnGridContextMenuRequested);
	connect(
		m_asset_grid_->verticalScrollBar(),
		&QScrollBar::valueChanged,
		this,
		&AssetBrowserPanelWidget::OnGridScrolled);
}

void AssetBrowserPanelWidget::RefreshFromRegistry()
{
	const uint64_t RegistryRevision = FAssetRegistry::Get().GetRevision();
	if (RegistryRevision == m_last_registry_revision_ && m_folder_tree_->topLevelItemCount() > 0)
	{
		return;
	}

	m_last_registry_revision_ = RegistryRevision;
	m_all_entries_ = FAssetRegistry::Get().ListAssets();
	RebuildFolderTree();
	ApplyFilters();
}

void AssetBrowserPanelWidget::RebuildFolderTree()
{
	m_folder_tree_->clear();
	const FAssetFolderNode RootNode = AssetFolderTreeBuilder::BuildFromRegistry(m_all_entries_);

	for (const std::unique_ptr<FAssetFolderNode>& Child : RootNode.Children)
	{
		auto* TopItem = new QTreeWidgetItem(m_folder_tree_, {QString::fromStdString(Child->DisplayName)});
		TopItem->setData(0, Qt::UserRole, QString::fromStdString(Child->Path));
		PopulateTreeItem(TopItem, *Child);
	}

	if (m_folder_tree_->topLevelItemCount() > 0)
	{
		m_folder_tree_->setCurrentItem(m_folder_tree_->topLevelItem(0));
	}
}

void AssetBrowserPanelWidget::ApplyFilters()
{
	std::vector<FAssetBrowserListItem> FilteredItems;
	FilteredItems.reserve(m_all_entries_.size());

	const QString SearchLower = m_search_text_.trimmed().toLower();
	for (const FAssetRegistryEntry& Entry : m_all_entries_)
	{
		if (!AssetFolderTreeBuilder::IsAssetInFolder(Entry, m_selected_folder_path_))
		{
			continue;
		}

		if (!m_type_filter_.isEmpty())
		{
			if (m_type_filter_ == QStringLiteral("__other__"))
			{
				if (Entry.Type == "StaticMesh" || Entry.Type == "SkeletalMesh")
				{
					continue;
				}
			}
			else if (QString::fromStdString(Entry.Type) != m_type_filter_)
			{
				continue;
			}
		}

		if (!SearchLower.isEmpty())
		{
			const QString ObjectName = QString::fromStdString(Entry.ObjectName).toLower();
			const QString AssetPath = QString::fromStdString(Entry.AssetPath).toLower();
			if (!ObjectName.contains(SearchLower) && !AssetPath.contains(SearchLower))
			{
				continue;
			}
		}

		FAssetBrowserListItem Item;
		Item.Entry = Entry;
		FilteredItems.push_back(std::move(Item));
	}

	std::sort(
		FilteredItems.begin(),
		FilteredItems.end(),
		[](const FAssetBrowserListItem& A, const FAssetBrowserListItem& B)
		{
			return A.Entry.ObjectName < B.Entry.ObjectName;
		});

	m_list_model_->SetItems(std::move(FilteredItems));
	m_asset_grid_->clearSelection();
	m_asset_grid_->setCurrentIndex(QModelIndex());
	m_asset_grid_->viewport()->setProperty("assetDragSourceRow", kAssetDragSourceNone);
	m_status_label_->setText(tr("%1 items").arg(m_list_model_->rowCount()));

	for (int Row = 0; Row < m_list_model_->rowCount(); ++Row)
	{
		RequestThumbnailForRow(Row, Row == 0);
	}
}

void AssetBrowserPanelWidget::RequestVisibleThumbnails()
{
	if (m_asset_grid_ == nullptr || m_list_model_ == nullptr)
	{
		return;
	}

	const QRect ViewportRect = m_asset_grid_->viewport()->rect();
	const QModelIndex TopLeft = m_asset_grid_->indexAt(ViewportRect.topLeft());
	const QModelIndex BottomRight = m_asset_grid_->indexAt(ViewportRect.bottomRight());
	if (!TopLeft.isValid())
	{
		return;
	}

	const int StartRow = TopLeft.row();
	const int EndRow = BottomRight.isValid() ? BottomRight.row() : (StartRow + 24);
	for (int Row = StartRow; Row <= EndRow && Row < m_list_model_->rowCount(); ++Row)
	{
		RequestThumbnailForRow(Row, Row == StartRow);
	}
}

void AssetBrowserPanelWidget::RequestThumbnailForRow(int InRow, bool bHighPriority)
{
	const FAssetBrowserListItem* Item = m_list_model_->GetItemAt(InRow);
	if (Item == nullptr || Item->bThumbnailPending || !Item->Thumbnail.isNull())
	{
		return;
	}

	QImage CachedImage;
	if (FAssetThumbnailService::Get().TryGetCached(Item->Entry, &CachedImage))
	{
		const QString CacheKey = m_list_model_->data(
			m_list_model_->index(InRow),
			static_cast<int>(EAssetListRole::CacheKey)).toString();
		m_list_model_->UpdateThumbnail(CacheKey, CachedImage);
		return;
	}

	m_list_model_->MarkThumbnailPending(InRow);
	FAssetThumbnailService::Get().RequestThumbnail(Item->Entry, kThumbnailSize, bHighPriority);
}

void AssetBrowserPanelWidget::OnFolderSelectionChanged()
{
	QTreeWidgetItem* CurrentItem = m_folder_tree_->currentItem();
	if (CurrentItem == nullptr)
	{
		m_selected_folder_path_ = "All";
	}
	else
	{
		m_selected_folder_path_ = CurrentItem->data(0, Qt::UserRole).toString().toStdString();
	}
	ApplyFilters();
}

void AssetBrowserPanelWidget::OnSearchTextChanged(const QString& InText)
{
	m_search_text_ = InText;
	ApplyFilters();
}

void AssetBrowserPanelWidget::OnTypeFilterChanged(int InIndex)
{
	m_type_filter_ = m_type_filter_combo_->itemData(InIndex).toString();
	ApplyFilters();
}

void AssetBrowserPanelWidget::OnGridContextMenuRequested(const QPoint& InPos)
{
	const QModelIndex Index = m_asset_grid_->indexAt(InPos);
	if (!Index.isValid())
	{
		return;
	}

	const QString SoftPath = Index.data(static_cast<int>(EAssetListRole::SoftObjectPath)).toString();
	const QString UAssetPath = Index.data(static_cast<int>(EAssetListRole::UAssetFilePath)).toString();
	const QString AssetType = Index.data(static_cast<int>(EAssetListRole::AssetType)).toString();

	QMenu Menu(this);
	QAction* ShowInExplorerAction = Menu.addAction(tr("在资源管理器中显示"));
	QAction* CopyPathAction = Menu.addAction(tr("复制 SoftObjectPath"));
	QAction* ReimportAction = nullptr;
	if (AssetTypeInfo::IsMeshAssetType(AssetType.toStdString()))
	{
		ReimportAction = Menu.addAction(tr("Reimport"));
	}

	QAction* Chosen = Menu.exec(m_asset_grid_->viewport()->mapToGlobal(InPos));
	if (Chosen == ShowInExplorerAction)
	{
		const QFileInfo FileInfo(UAssetPath);
		QDesktopServices::openUrl(QUrl::fromLocalFile(FileInfo.absolutePath()));
	}
	else if (Chosen == CopyPathAction)
	{
		QApplication::clipboard()->setText(SoftPath);
	}
	else if (Chosen == ReimportAction)
	{
		const FAssetBrowserListItem* Item = m_list_model_->GetItemAt(Index.row());
		if (Item == nullptr || Item->Entry.SourceFile.empty())
		{
			QMessageBox::warning(this, tr("Reimport"), tr("该资产缺少 SourceFile，无法 Reimport。"));
			return;
		}

		std::string ErrorMessage;
		const std::filesystem::path SourcePath = Item->Entry.SourceFile;
		if (!UMeshImportFactory::Reimport(SoftPath.toStdString(), SourcePath, &ErrorMessage))
		{
			QMessageBox::warning(
				this,
				tr("Reimport 失败"),
				tr("无法 Reimport: %1").arg(QString::fromStdString(ErrorMessage)));
			return;
		}

		FAssetThumbnailService::Get().InvalidateEntry(Item->Entry);
		RefreshFromRegistry();
	}
}

void AssetBrowserPanelWidget::OnThumbnailReady(const QString& InCacheKey, const QImage& InImage)
{
	if (InImage.isNull())
	{
		m_list_model_->ClearThumbnailPending(InCacheKey);
		return;
	}
	m_list_model_->UpdateThumbnail(InCacheKey, InImage);
}

void AssetBrowserPanelWidget::OnGridScrolled()
{
	RequestVisibleThumbnails();
}
