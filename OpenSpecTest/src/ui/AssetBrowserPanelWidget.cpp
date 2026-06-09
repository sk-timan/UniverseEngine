#include "ui/AssetBrowserPanelWidget.h"

#include <algorithm>
#include <filesystem>
#include <functional>

#include <QAction>
#include <QApplication>
#include <QAbstractItemView>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QCursor>
#include <QDateTime>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QPainter>
#include <QInputDialog>
#include <QKeyEvent>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QScrollBar>
#include <QShowEvent>
#include <QSet>
#include <QSplitter>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "app/GameApp.h"
#include "asset/AssetFolderTree.h"
#include "asset/AssetRegistry.h"
#include "asset/AssetDuplicateService.h"
#include "asset/AssetRenameService.h"
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
constexpr int kContentDiskPollIntervalMs = 1000;
constexpr int kContentDiskRescanDebounceMs = 800;

QString FsPathToQString(const std::filesystem::path& InPath)
{
#ifdef _WIN32
	return QString::fromStdWString(InPath.wstring());
#else
	const std::u8string U8Path = InPath.u8string();
	return QString::fromUtf8(reinterpret_cast<const char*>(U8Path.data()), static_cast<int>(U8Path.size()));
#endif
}

QString Utf8StdStringToQString(const std::string& InUtf8Text)
{
	return QString::fromUtf8(InUtf8Text.c_str(), static_cast<int>(InUtf8Text.size()));
}

std::filesystem::path QStringToFsPath(const QString& InPath)
{
#ifdef _WIN32
	return std::filesystem::path(InPath.toStdWString());
#else
	return std::filesystem::path(InPath.toUtf8().toStdString());
#endif
}

bool IsImportableModelExtension(const QString& InExtension)
{
	static const QSet<QString> SupportedExtensions = {
		QStringLiteral("fbx"),
		QStringLiteral("obj"),
		QStringLiteral("gltf"),
		QStringLiteral("glb"),
		QStringLiteral("dae"),
		QStringLiteral("3ds"),
		QStringLiteral("blend"),
	};
	return SupportedExtensions.contains(InExtension.toLower());
}

bool IsImportableModelFile(const std::filesystem::path& InPath)
{
	if (!InPath.has_extension())
	{
		return false;
	}

	const QString Extension = QString::fromStdString(InPath.extension().string());
	return IsImportableModelExtension(Extension.startsWith('.') ? Extension.mid(1) : Extension);
}

bool ExtractImportableModelFiles(const QList<QUrl>& InFileUrls, std::vector<std::filesystem::path>* OutPaths)
{
	if (OutPaths == nullptr)
	{
		return false;
	}

	OutPaths->clear();
	for (const QUrl& FileUrl : InFileUrls)
	{
		if (!FileUrl.isLocalFile())
		{
			continue;
		}

		const std::filesystem::path LocalPath = QStringToFsPath(FileUrl.toLocalFile());
		if (!std::filesystem::is_regular_file(LocalPath) || !IsImportableModelFile(LocalPath))
		{
			continue;
		}

		OutPaths->push_back(LocalPath);
	}

	return !OutPaths->empty();
}

uint64_t CombineFingerprint(uint64_t InSeed, uint64_t InValue)
{
	return InSeed ^ (InValue + 0x9e3779b97f4a7c15ULL + (InSeed << 6) + (InSeed >> 2));
}

uint64_t HashUtf8Text(uint64_t InSeed, const std::string& InUtf8Text)
{
	uint64_t Hash = InSeed;
	for (const unsigned char Character : InUtf8Text)
	{
		Hash = CombineFingerprint(Hash, Character);
	}
	return Hash;
}

void WalkContentDiskEntries(
	const std::function<void(const std::filesystem::path&, bool bIsDirectory, uint64_t InWriteTimeValue)>&
		InVisitor)
{
	if (!std::filesystem::exists(GProjectContentDirectory))
	{
		return;
	}

	std::error_code ErrorCode;
	for (const auto& DirectoryEntry : std::filesystem::recursive_directory_iterator(
		GProjectContentDirectory,
		std::filesystem::directory_options::skip_permission_denied,
		ErrorCode))
	{
		const auto WriteTime = std::filesystem::last_write_time(DirectoryEntry.path(), ErrorCode);
		if (ErrorCode)
		{
			continue;
		}

		const uint64_t WriteTimeValue = static_cast<uint64_t>(WriteTime.time_since_epoch().count());
		if (DirectoryEntry.is_directory())
		{
			InVisitor(DirectoryEntry.path(), true, WriteTimeValue);
			continue;
		}

		if (DirectoryEntry.is_regular_file() && DirectoryEntry.path().extension() == ".uasset")
		{
			InVisitor(DirectoryEntry.path(), false, WriteTimeValue);
		}
	}
}

uint64_t ComputeUAssetDiskFingerprint()
{
	uint64_t Fingerprint = 0;
	WalkContentDiskEntries([&Fingerprint](const std::filesystem::path&, bool bIsDirectory, uint64_t InWriteTimeValue)
	{
		if (!bIsDirectory)
		{
			Fingerprint = CombineFingerprint(Fingerprint, InWriteTimeValue);
		}
	});
	return Fingerprint;
}

uint64_t ComputeDirectoryDiskFingerprint()
{
	uint64_t Fingerprint = 0;
	WalkContentDiskEntries([&Fingerprint](const std::filesystem::path& InPath, bool bIsDirectory, uint64_t InWriteTimeValue)
	{
		if (bIsDirectory)
		{
			Fingerprint = HashUtf8Text(Fingerprint, FsPathUtf8Generic(InPath));
			Fingerprint = CombineFingerprint(Fingerprint, InWriteTimeValue);
		}
	});
	return Fingerprint;
}

uint32_t CountContentDirectories()
{
	uint32_t Count = 0;
	WalkContentDiskEntries([&Count](const std::filesystem::path&, bool bIsDirectory, uint64_t)
	{
		if (bIsDirectory)
		{
			++Count;
		}
	});
	return Count;
}

class AssetFolderTreeWidget final : public QTreeWidget
{
public:
	explicit AssetFolderTreeWidget(QWidget* InParent = nullptr)
		: QTreeWidget(InParent)
	{
		setAcceptDrops(true);
		setDropIndicatorShown(false);
	}

	std::function<void(const std::string& InTargetFolderPath, const std::string& InSoftObjectPath)> AssetDropHandler;

protected:
	bool IsValidAssetDropTargetItem(const QTreeWidgetItem* InItem) const
	{
		if (InItem == nullptr)
		{
			return false;
		}

		const std::string FolderPath = InItem->data(0, Qt::UserRole).toString().toStdString();
		return !FolderPath.empty() && FolderPath != "All";
	}

	void ClearAssetDropHoverHighlight()
	{
		if (m_asset_drop_hover_item_ == nullptr)
		{
			return;
		}

		m_asset_drop_hover_item_->setData(0, Qt::BackgroundRole, QVariant());
		m_asset_drop_hover_item_ = nullptr;
		viewport()->update();
	}

	void UpdateAssetDropHoverHighlight(QTreeWidgetItem* InItem)
	{
		if (InItem == m_asset_drop_hover_item_)
		{
			return;
		}

		ClearAssetDropHoverHighlight();
		if (!IsValidAssetDropTargetItem(InItem))
		{
			return;
		}

		m_asset_drop_hover_item_ = InItem;
		m_asset_drop_hover_item_->setData(0, Qt::BackgroundRole, QColor("#2a2d2e"));
		viewport()->update();
	}

	void dragEnterEvent(QDragEnterEvent* InEvent) override
	{
		if (InEvent->mimeData()->hasFormat(kSoftObjectPathMimeType))
		{
			InEvent->setDropAction(Qt::CopyAction);
			InEvent->accept();
			UpdateAssetDropHoverHighlight(itemAt(InEvent->position().toPoint()));
			return;
		}

		QTreeWidget::dragEnterEvent(InEvent);
	}

	void dragMoveEvent(QDragMoveEvent* InEvent) override
	{
		if (InEvent->mimeData()->hasFormat(kSoftObjectPathMimeType))
		{
			QTreeWidgetItem* TargetItem = itemAt(InEvent->position().toPoint());
			if (IsValidAssetDropTargetItem(TargetItem))
			{
				InEvent->setDropAction(Qt::CopyAction);
				InEvent->accept();
				UpdateAssetDropHoverHighlight(TargetItem);
			}
			else
			{
				ClearAssetDropHoverHighlight();
				InEvent->ignore();
			}
			return;
		}

		QTreeWidget::dragMoveEvent(InEvent);
	}

	void dragLeaveEvent(QDragLeaveEvent* InEvent) override
	{
		ClearAssetDropHoverHighlight();
		QTreeWidget::dragLeaveEvent(InEvent);
	}

	void dropEvent(QDropEvent* InEvent) override
	{
		ClearAssetDropHoverHighlight();

		if (!InEvent->mimeData()->hasFormat(kSoftObjectPathMimeType))
		{
			QTreeWidget::dropEvent(InEvent);
			return;
		}

		QTreeWidgetItem* TargetItem = itemAt(InEvent->position().toPoint());
		if (!IsValidAssetDropTargetItem(TargetItem))
		{
			InEvent->ignore();
			return;
		}

		const std::string TargetFolderPath = TargetItem->data(0, Qt::UserRole).toString().toStdString();
		const std::string SoftPath =
			QString::fromUtf8(InEvent->mimeData()->data(kSoftObjectPathMimeType)).toStdString();
		if (AssetDropHandler)
		{
			AssetDropHandler(TargetFolderPath, SoftPath);
		}

		InEvent->setDropAction(Qt::CopyAction);
		InEvent->accept();
	}

private:
	QTreeWidgetItem* m_asset_drop_hover_item_ = nullptr;
};

class AssetBrowserListView final : public QListView
{
public:
	explicit AssetBrowserListView(QWidget* InParent = nullptr)
		: QListView(InParent)
	{
		viewport()->installEventFilter(this);
	}

	std::function<void(const QList<QUrl>&)> ExternalDropHandler;
	std::function<void(const std::string& InTargetFolderPath, const std::string& InSoftObjectPath)> AssetDropHandler;

protected:
	QModelIndex FindFolderIndexAt(const QPoint& InViewportPos) const
	{
		const QModelIndex HitIndex = indexAt(InViewportPos);
		if (HitIndex.isValid() && HitIndex.data(static_cast<int>(EAssetListRole::IsFolder)).toBool())
		{
			return HitIndex;
		}

		if (model() == nullptr)
		{
			return {};
		}

		for (int Row = 0; Row < model()->rowCount(); ++Row)
		{
			const QModelIndex RowIndex = model()->index(Row, 0);
			if (!RowIndex.isValid() || !RowIndex.data(static_cast<int>(EAssetListRole::IsFolder)).toBool())
			{
				continue;
			}

			if (visualRect(RowIndex).contains(InViewportPos))
			{
				return RowIndex;
			}
		}

		return {};
	}

	bool IsFolderDropTargetAt(const QPoint& InViewportPos) const
	{
		return FindFolderIndexAt(InViewportPos).isValid();
	}

	void AcceptAssetMoveDrop(QDropEvent* InEvent, const QPoint& InViewportPos)
	{
		const QModelIndex Index = FindFolderIndexAt(InViewportPos);
		if (!Index.isValid())
		{
			return;
		}

		const std::string TargetFolderPath =
			Index.data(static_cast<int>(EAssetListRole::FolderPath)).toString().toStdString();
		const std::string SoftPath =
			QString::fromUtf8(InEvent->mimeData()->data(kSoftObjectPathMimeType)).toStdString();
		if (!TargetFolderPath.empty() && AssetDropHandler)
		{
			AssetDropHandler(TargetFolderPath, SoftPath);
		}

		InEvent->setDropAction(Qt::CopyAction);
		InEvent->accept();
	}

	bool eventFilter(QObject* InWatched, QEvent* InEvent) override
	{
		if (InWatched != viewport())
		{
			return QListView::eventFilter(InWatched, InEvent);
		}

		if (InEvent->type() == QEvent::DragEnter)
		{
			auto* DragEnterEvent = static_cast<QDragEnterEvent*>(InEvent);
			std::vector<std::filesystem::path> ImportablePaths;
			if (DragEnterEvent->mimeData()->hasUrls()
				&& ExtractImportableModelFiles(DragEnterEvent->mimeData()->urls(), &ImportablePaths))
			{
				DragEnterEvent->acceptProposedAction();
				return true;
			}

			if (DragEnterEvent->mimeData()->hasFormat(kSoftObjectPathMimeType))
			{
				DragEnterEvent->setDropAction(Qt::CopyAction);
				DragEnterEvent->accept();
				return true;
			}

			return false;
		}

		if (InEvent->type() == QEvent::DragMove)
		{
			auto* DragMoveEvent = static_cast<QDragMoveEvent*>(InEvent);
			std::vector<std::filesystem::path> ImportablePaths;
			if (DragMoveEvent->mimeData()->hasUrls()
				&& ExtractImportableModelFiles(DragMoveEvent->mimeData()->urls(), &ImportablePaths))
			{
				DragMoveEvent->acceptProposedAction();
				return true;
			}

			if (DragMoveEvent->mimeData()->hasFormat(kSoftObjectPathMimeType))
			{
				if (IsFolderDropTargetAt(DragMoveEvent->position().toPoint()))
				{
					DragMoveEvent->setDropAction(Qt::CopyAction);
					DragMoveEvent->accept();
				}
				else
				{
					DragMoveEvent->ignore();
				}
				return true;
			}

			return false;
		}

		if (InEvent->type() == QEvent::Drop)
		{
			auto* DropEvent = static_cast<QDropEvent*>(InEvent);
			std::vector<std::filesystem::path> ImportablePaths;
			if (DropEvent->mimeData()->hasUrls()
				&& ExtractImportableModelFiles(DropEvent->mimeData()->urls(), &ImportablePaths))
			{
				if (ExternalDropHandler)
				{
					ExternalDropHandler(DropEvent->mimeData()->urls());
				}
				DropEvent->acceptProposedAction();
				return true;
			}

			if (DropEvent->mimeData()->hasFormat(kSoftObjectPathMimeType))
			{
				if (IsFolderDropTargetAt(DropEvent->position().toPoint()))
				{
					AcceptAssetMoveDrop(DropEvent, DropEvent->position().toPoint());
				}
				else
				{
					DropEvent->ignore();
				}
				return true;
			}

			return false;
		}

		return QListView::eventFilter(InWatched, InEvent);
	}

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

		const Qt::DropAction Result = Drag->exec(Qt::CopyAction, Qt::CopyAction);

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
		auto* Item = new QTreeWidgetItem(InParentItem, {Utf8StdStringToQString(Child->DisplayName)});
		Item->setData(0, Qt::UserRole, Utf8StdStringToQString(Child->Path));
		const bool bCanRenameFolder =
			Child->Path != "All" && Child->Path != "Content";
		if (bCanRenameFolder)
		{
			Item->setFlags(Item->flags() | Qt::ItemIsEditable);
		}
		PopulateTreeItem(Item, *Child);
	}
}
} // namespace

AssetBrowserPanelWidget::AssetBrowserPanelWidget(GameApp* InGameApp, QWidget* InParent)
	: QWidget(InParent)
	, m_game_app_(InGameApp)
{
	BuildUi();
	SetupContentDiskWatcher();
	connect(
		&FAssetThumbnailService::Get(),
		&FAssetThumbnailService::ThumbnailReady,
		this,
		&AssetBrowserPanelWidget::OnThumbnailReady);
	RefreshFromRegistry();
}

void AssetBrowserPanelWidget::showEvent(QShowEvent* InEvent)
{
	QWidget::showEvent(InEvent);
	if (!m_b_initial_splitter_applied_)
	{
		ApplyInitialSplitterProportions();
		m_b_initial_splitter_applied_ = true;
	}
}

void AssetBrowserPanelWidget::ApplyInitialSplitterProportions()
{
	if (m_splitter_ == nullptr)
	{
		return;
	}

	const int TotalWidth = m_splitter_->width();
	if (TotalWidth <= 0)
	{
		return;
	}

	const int FolderWidth = TotalWidth * 20 / 100;
	m_splitter_->setSizes({FolderWidth, TotalWidth - FolderWidth});
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
	m_type_filter_combo_->addItem(tr("无"), QStringLiteral("__none__"));
	m_type_filter_combo_->addItem(tr("全部类型"), QString());
	m_type_filter_combo_->addItem(tr("Static Mesh"), QStringLiteral("StaticMesh"));
	m_type_filter_combo_->addItem(tr("Skeletal Mesh"), QStringLiteral("SkeletalMesh"));
	m_type_filter_combo_->addItem(tr("其它"), QStringLiteral("__other__"));
	m_type_filter_ = QStringLiteral("__none__");
	ToolbarLayout->addWidget(m_type_filter_combo_);
	RootLayout->addLayout(ToolbarLayout);

	m_splitter_ = new QSplitter(Qt::Horizontal, this);
	m_folder_tree_ = new AssetFolderTreeWidget(m_splitter_);
	m_folder_tree_->setHeaderHidden(true);
	m_folder_tree_->setMinimumWidth(120);
	m_folder_tree_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_folder_tree_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_folder_tree_->setContextMenuPolicy(Qt::CustomContextMenu);
	static_cast<AssetFolderTreeWidget*>(m_folder_tree_)->AssetDropHandler =
		[this](const std::string& InTargetFolderPath, const std::string& InSoftObjectPath)
		{
			OnAssetDroppedToFolder(InTargetFolderPath, InSoftObjectPath);
		};

	m_asset_grid_ = new AssetBrowserListView(m_splitter_);
	m_asset_grid_->setObjectName("AssetBrowserGrid");
	m_asset_grid_->setViewMode(QListView::IconMode);
	m_asset_grid_->setResizeMode(QListView::Adjust);
	m_asset_grid_->setMovement(QListView::Static);
	m_asset_grid_->setSpacing(8);
	m_asset_grid_->setSelectionMode(QAbstractItemView::SingleSelection);
	m_asset_grid_->setFocusPolicy(Qt::ClickFocus);
	m_asset_grid_->setDragEnabled(true);
	m_asset_grid_->setAcceptDrops(true);
	m_asset_grid_->setDragDropMode(QAbstractItemView::DragDrop);
	m_asset_grid_->setDefaultDropAction(Qt::CopyAction);
	m_asset_grid_->viewport()->setAcceptDrops(true);
	m_asset_grid_->setContextMenuPolicy(Qt::CustomContextMenu);
	m_asset_grid_->setUniformItemSizes(true);
	m_asset_grid_->setWrapping(true);
	m_asset_grid_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_asset_grid_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	m_list_model_ = new AssetListModel(this);
	m_asset_grid_->setModel(m_list_model_);
	m_asset_grid_->setItemDelegate(new AssetTileDelegate(m_asset_grid_));
	m_asset_grid_->viewport()->setProperty("assetDragSourceRow", kAssetDragSourceNone);
	static_cast<AssetBrowserListView*>(m_asset_grid_)->ExternalDropHandler =
		[this](const QList<QUrl>& InFileUrls)
		{
			OnExternalFilesDropped(InFileUrls);
		};
	static_cast<AssetBrowserListView*>(m_asset_grid_)->AssetDropHandler =
		[this](const std::string& InTargetFolderPath, const std::string& InSoftObjectPath)
		{
			OnAssetDroppedToFolder(InTargetFolderPath, InSoftObjectPath);
		};

	m_splitter_->addWidget(m_folder_tree_);
	m_splitter_->addWidget(m_asset_grid_);
	m_splitter_->setStretchFactor(0, 2);
	m_splitter_->setStretchFactor(1, 8);
	m_splitter_->setSizes({296, 1184});
	RootLayout->addWidget(m_splitter_, 1);

	m_status_label_ = new QLabel(tr("0 items"), this);
	m_status_label_->setObjectName("SecondaryLabel");
	RootLayout->addWidget(m_status_label_);

	m_rename_action_ = new QAction(tr("重命名"), this);
	m_rename_action_->setShortcut(QKeySequence(Qt::Key_F2));
	m_rename_action_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	addAction(m_rename_action_);

	m_copy_action_ = new QAction(tr("复制"), this);
	m_copy_action_->setShortcut(QKeySequence::Copy);
	m_copy_action_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	addAction(m_copy_action_);

	m_paste_action_ = new QAction(tr("粘贴"), this);
	m_paste_action_->setShortcut(QKeySequence::Paste);
	m_paste_action_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	m_paste_action_->setEnabled(false);
	addAction(m_paste_action_);

	m_folder_tree_->installEventFilter(this);
	m_asset_grid_->installEventFilter(this);

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
		m_folder_tree_,
		&QTreeWidget::itemChanged,
		this,
		&AssetBrowserPanelWidget::OnFolderItemChanged);
	connect(
		m_folder_tree_,
		&QTreeWidget::customContextMenuRequested,
		this,
		&AssetBrowserPanelWidget::OnFolderTreeContextMenuRequested);
	connect(
		m_asset_grid_,
		&QListView::customContextMenuRequested,
		this,
		&AssetBrowserPanelWidget::OnGridContextMenuRequested);
	connect(
		m_asset_grid_,
		&QListView::doubleClicked,
		this,
		&AssetBrowserPanelWidget::OnAssetGridDoubleClicked);
	connect(
		m_asset_grid_->verticalScrollBar(),
		&QScrollBar::valueChanged,
		this,
		&AssetBrowserPanelWidget::OnGridScrolled);
	connect(m_rename_action_, &QAction::triggered, this, [this]()
	{
		if (m_asset_grid_->hasFocus())
		{
			BeginRenameSelectedAsset();
			return;
		}
		BeginRenameSelectedFolder();
	});
	connect(m_copy_action_, &QAction::triggered, this, [this]()
	{
		if (m_asset_grid_->hasFocus())
		{
			CopySelectedAsset();
		}
	});
	connect(m_paste_action_, &QAction::triggered, this, [this]()
	{
		if (m_asset_grid_->hasFocus())
		{
			PasteCopiedAsset();
		}
	});
}

bool AssetBrowserPanelWidget::eventFilter(QObject* InWatched, QEvent* InEvent)
{
	if (InEvent->type() == QEvent::KeyPress)
	{
		auto* KeyEvent = static_cast<QKeyEvent*>(InEvent);
		if (KeyEvent->key() == Qt::Key_F2)
		{
			if (InWatched == m_folder_tree_)
			{
				BeginRenameSelectedFolder();
				return true;
			}
			if (InWatched == m_asset_grid_)
			{
				BeginRenameSelectedAsset();
				return true;
			}
		}

		if (InWatched == m_asset_grid_)
		{
			if (KeyEvent->matches(QKeySequence::Copy))
			{
				CopySelectedAsset();
				return true;
			}
			if (KeyEvent->matches(QKeySequence::Paste) && m_paste_action_->isEnabled())
			{
				PasteCopiedAsset();
				return true;
			}
		}
	}

	return QWidget::eventFilter(InWatched, InEvent);
}

bool AssetBrowserPanelWidget::CanRenameFolderItem(const QTreeWidgetItem* InItem) const
{
	if (InItem == nullptr)
	{
		return false;
	}

	const std::string FolderPath = InItem->data(0, Qt::UserRole).toString().toStdString();
	return !FolderPath.empty() && FolderPath != "All" && FolderPath != "Content";
}

bool AssetBrowserPanelWidget::CanCreateFolderUnderItem(const QTreeWidgetItem* InItem) const
{
	if (InItem == nullptr)
	{
		return false;
	}

	const std::string FolderPath = InItem->data(0, Qt::UserRole).toString().toStdString();
	return !FolderPath.empty() && FolderPath != "All";
}

bool AssetBrowserPanelWidget::CanImportIntoSelectedFolder() const
{
	return !m_selected_folder_path_.empty() && m_selected_folder_path_ != "All";
}

std::string AssetBrowserPanelWidget::BuildImportAssetPath(const std::string& InModelName) const
{
	if (m_selected_folder_path_ == "Content")
	{
		return InModelName;
	}

	return m_selected_folder_path_ + "/" + InModelName;
}

void AssetBrowserPanelWidget::OnExternalFilesDropped(const QList<QUrl>& InFileUrls)
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	if (!CanImportIntoSelectedFolder())
	{
		QMessageBox::warning(
			this,
			tr("无法导入"),
			tr("请先在左侧选择一个目标文件夹（不能为 All）。"));
		return;
	}

	std::vector<std::filesystem::path> ImportablePaths;
	if (!ExtractImportableModelFiles(InFileUrls, &ImportablePaths))
	{
		QMessageBox::warning(
			this,
			tr("无法导入"),
			tr("仅支持拖入 3D 模型文件（fbx、obj、gltf、glb、dae、3ds、blend）。"));
		return;
	}

	int SuccessCount = 0;
	QStringList FailedMessages;
	for (const std::filesystem::path& SourcePath : ImportablePaths)
	{
		const QString ModelName = QFileInfo(FsPathToQString(SourcePath)).completeBaseName();
		if (ModelName.trimmed().isEmpty())
		{
			FailedMessages.push_back(tr("%1: 无效文件名").arg(FsPathToQString(SourcePath)));
			continue;
		}

		const std::string ContentAssetPath = BuildImportAssetPath(ModelName.toUtf8().toStdString());
		std::string SoftObjectPath;
		std::string ErrorMessage;
		if (!m_game_app_->ImportAssetFromSourceFile(
				SourcePath,
				ContentAssetPath,
				&SoftObjectPath,
				&ErrorMessage))
		{
			FailedMessages.push_back(
				tr("%1: %2").arg(ModelName).arg(QString::fromStdString(ErrorMessage)));
			continue;
		}

		++SuccessCount;
	}

	if (SuccessCount > 0)
	{
		m_last_registry_revision_ = 0;
		m_uasset_disk_fingerprint_ = ComputeUAssetDiskFingerprint();
		m_directory_disk_fingerprint_ = ComputeDirectoryDiskFingerprint();
		MaybeUpdateContentDiskWatchPaths();
		RefreshFromRegistry();
	}

	if (!FailedMessages.isEmpty())
	{
		QMessageBox::warning(
			this,
			SuccessCount > 0 ? tr("部分导入失败") : tr("导入失败"),
			FailedMessages.join('\n'));
	}
}

void AssetBrowserPanelWidget::BeginRenameSelectedFolder()
{
	QTreeWidgetItem* CurrentItem = m_folder_tree_->currentItem();
	if (!CanRenameFolderItem(CurrentItem))
	{
		return;
	}

	m_folder_tree_->setFocus();
	m_folder_tree_->editItem(CurrentItem, 0);
}

void AssetBrowserPanelWidget::OnFolderItemChanged(QTreeWidgetItem* InItem, int InColumn)
{
	(void)InColumn;
	if (m_is_folder_rename_in_progress_ || InItem == nullptr || !CanRenameFolderItem(InItem))
	{
		return;
	}

	const std::string FolderPath = InItem->data(0, Qt::UserRole).toString().toStdString();
	const std::string NewFolderName = InItem->text(0).trimmed().toStdString();
	if (NewFolderName.empty())
	{
		m_last_registry_revision_ = 0;
		RebuildFolderTree();
		return;
	}

	const size_t LastSlash = FolderPath.find_last_of('/');
	const std::string OldFolderName =
		(LastSlash == std::string::npos) ? FolderPath : FolderPath.substr(LastSlash + 1);
	if (NewFolderName == OldFolderName)
	{
		return;
	}

	std::string ErrorMessage;
	if (!FAssetRenameService::RenameFolder(FolderPath, NewFolderName, &ErrorMessage))
	{
		QMessageBox::warning(
			this,
			tr("重命名失败"),
			tr("无法重命名文件夹: %1").arg(QString::fromStdString(ErrorMessage)));
		m_last_registry_revision_ = 0;
		RebuildFolderTree();
		return;
	}

	const std::string ParentPath = (LastSlash == std::string::npos) ? std::string() : FolderPath.substr(0, LastSlash);
	m_selected_folder_path_ =
		ParentPath.empty() ? NewFolderName : ParentPath + "/" + NewFolderName;
	m_last_registry_revision_ = 0;
	RefreshFromRegistry();
}

QTreeWidgetItem* AssetBrowserPanelWidget::FindFolderTreeItemByPath(const std::string& InFolderPath) const
{
	if (m_folder_tree_ == nullptr)
	{
		return nullptr;
	}

	std::function<QTreeWidgetItem*(QTreeWidgetItem*)> FindByPath;
	FindByPath = [&](QTreeWidgetItem* InItem) -> QTreeWidgetItem*
	{
		if (InItem == nullptr)
		{
			return nullptr;
		}
		if (InItem->data(0, Qt::UserRole).toString().toStdString() == InFolderPath)
		{
			return InItem;
		}
		for (int ChildIndex = 0; ChildIndex < InItem->childCount(); ++ChildIndex)
		{
			if (QTreeWidgetItem* FoundItem = FindByPath(InItem->child(ChildIndex)))
			{
				return FoundItem;
			}
		}
		return nullptr;
	};

	for (int TopIndex = 0; TopIndex < m_folder_tree_->topLevelItemCount(); ++TopIndex)
	{
		if (QTreeWidgetItem* FoundItem = FindByPath(m_folder_tree_->topLevelItem(TopIndex)))
		{
			return FoundItem;
		}
	}

	return nullptr;
}

void AssetBrowserPanelWidget::SelectFolderTreeItemByPath(const std::string& InFolderPath)
{
	if (m_folder_tree_ == nullptr)
	{
		return;
	}

	if (QTreeWidgetItem* FoundItem = FindFolderTreeItemByPath(InFolderPath))
	{
		for (QTreeWidgetItem* ParentItem = FoundItem->parent(); ParentItem != nullptr; ParentItem = ParentItem->parent())
		{
			ParentItem->setExpanded(true);
		}
		m_is_folder_rename_in_progress_ = true;
		m_folder_tree_->setCurrentItem(FoundItem);
		m_is_folder_rename_in_progress_ = false;
		return;
	}

	if (m_folder_tree_->topLevelItemCount() > 0)
	{
		m_folder_tree_->setCurrentItem(m_folder_tree_->topLevelItem(0));
	}
}

void AssetBrowserPanelWidget::BeginRenameSelectedAsset()
{
	const QModelIndex CurrentIndex = m_asset_grid_->currentIndex();
	if (!CurrentIndex.isValid())
	{
		return;
	}

	const FAssetBrowserListItem* Item = m_list_model_->GetItemAt(CurrentIndex.row());
	if (Item == nullptr || Item->bIsFolder)
	{
		return;
	}

	const QString CurrentName = QString::fromStdString(Item->Entry.ObjectName);
	const QString NewName = QInputDialog::getText(
		this,
		tr("重命名资产"),
		tr("新资产名称:"),
		QLineEdit::Normal,
		CurrentName);
	if (NewName.trimmed().isEmpty() || NewName.trimmed() == CurrentName.trimmed())
	{
		return;
	}

	std::string ErrorMessage;
	if (!FAssetRenameService::RenameAssetObject(Item->Entry, NewName.trimmed().toStdString(), &ErrorMessage))
	{
		QMessageBox::warning(
			this,
			tr("重命名失败"),
			tr("无法重命名资产: %1").arg(QString::fromStdString(ErrorMessage)));
		return;
	}

	m_last_registry_revision_ = 0;
	RefreshFromRegistry();
}

void AssetBrowserPanelWidget::CopySelectedAsset()
{
	const QModelIndex CurrentIndex = m_asset_grid_->currentIndex();
	if (!CurrentIndex.isValid())
	{
		return;
	}

	const FAssetBrowserListItem* Item = m_list_model_->GetItemAt(CurrentIndex.row());
	if (Item == nullptr || Item->bIsFolder)
	{
		return;
	}

	m_copied_asset_entry_ = Item->Entry;
	if (m_paste_action_ != nullptr)
	{
		m_paste_action_->setEnabled(true);
	}
}

void AssetBrowserPanelWidget::PasteCopiedAsset()
{
	if (!m_copied_asset_entry_.has_value())
	{
		return;
	}

	if (!CanImportIntoSelectedFolder())
	{
		QMessageBox::warning(
			this,
			tr("无法粘贴"),
			tr("请先在左侧选择一个目标文件夹（不能为 All）。"));
		return;
	}

	std::string NewSoftObjectPath;
	std::string ErrorMessage;
	if (!FAssetDuplicateService::DuplicateAsset(
			m_copied_asset_entry_.value(),
			m_selected_folder_path_,
			&NewSoftObjectPath,
			&ErrorMessage))
	{
		QMessageBox::warning(
			this,
			tr("粘贴失败"),
			tr("无法创建资产副本: %1").arg(QString::fromStdString(ErrorMessage)));
		return;
	}

	RefreshAfterAssetDiskMutation();
}

void AssetBrowserPanelWidget::DuplicateSelectedAsset()
{
	const QModelIndex CurrentIndex = m_asset_grid_->currentIndex();
	if (!CurrentIndex.isValid())
	{
		return;
	}

	const FAssetBrowserListItem* Item = m_list_model_->GetItemAt(CurrentIndex.row());
	if (Item == nullptr || Item->bIsFolder)
	{
		return;
	}

	if (!CanImportIntoSelectedFolder())
	{
		QMessageBox::warning(
			this,
			tr("无法创建副本"),
			tr("请先在左侧选择一个目标文件夹（不能为 All）。"));
		return;
	}

	std::string NewSoftObjectPath;
	std::string ErrorMessage;
	if (!FAssetDuplicateService::DuplicateAsset(
			Item->Entry,
			m_selected_folder_path_,
			&NewSoftObjectPath,
			&ErrorMessage))
	{
		QMessageBox::warning(
			this,
			tr("创建副本失败"),
			tr("无法创建资产副本: %1").arg(QString::fromStdString(ErrorMessage)));
		return;
	}

	RefreshAfterAssetDiskMutation();
}

void AssetBrowserPanelWidget::OnAssetDroppedToFolder(
	const std::string& InTargetFolderPath,
	const std::string& InSoftObjectPath)
{
	const std::optional<FAssetRegistryEntry> Entry = FAssetRegistry::Get().FindBySoftPath(InSoftObjectPath);
	if (!Entry.has_value())
	{
		return;
	}

	std::string ErrorMessage;
	if (!FAssetRenameService::MoveAssetToFolder(Entry.value(), InTargetFolderPath, nullptr, &ErrorMessage))
	{
		QMessageBox::warning(
			this,
			tr("移动失败"),
			tr("无法移动资产: %1").arg(QString::fromStdString(ErrorMessage)));
		return;
	}

	m_selected_folder_path_ = InTargetFolderPath;
	RefreshAfterAssetDiskMutation();
	SelectFolderTreeItemByPath(InTargetFolderPath);
}

void AssetBrowserPanelWidget::RefreshAfterAssetDiskMutation()
{
	m_last_registry_revision_ = 0;
	m_uasset_disk_fingerprint_ = ComputeUAssetDiskFingerprint();
	m_directory_disk_fingerprint_ = ComputeDirectoryDiskFingerprint();
	MaybeUpdateContentDiskWatchPaths();
	RefreshFromRegistry();
}

void AssetBrowserPanelWidget::SetupContentDiskWatcher()
{
	m_uasset_disk_fingerprint_ = ComputeUAssetDiskFingerprint();
	m_directory_disk_fingerprint_ = ComputeDirectoryDiskFingerprint();
	m_watched_directory_count_ = CountContentDirectories();

	m_content_disk_watcher_ = new QFileSystemWatcher(this);
	m_content_rescan_timer_ = new QTimer(this);
	m_content_rescan_timer_->setSingleShot(true);
	m_content_rescan_timer_->setInterval(kContentDiskRescanDebounceMs);

	connect(
		m_content_rescan_timer_,
		&QTimer::timeout,
		this,
		&AssetBrowserPanelWidget::OnDebouncedContentDiskRescan);
	connect(
		m_content_disk_watcher_,
		&QFileSystemWatcher::directoryChanged,
		this,
		[this](const QString&)
		{
			if (m_content_rescan_timer_ != nullptr)
			{
				m_content_rescan_timer_->start();
			}
		});

	UpdateContentDiskWatchPaths();
}

void AssetBrowserPanelWidget::UpdateContentDiskWatchPaths()
{
	if (m_content_disk_watcher_ == nullptr)
	{
		return;
	}

	QSet<QString> DesiredPaths;
	DesiredPaths.insert(FsPathToQString(GProjectContentDirectory));

	std::error_code ErrorCode;
	if (std::filesystem::exists(GProjectContentDirectory))
	{
		for (const auto& DirectoryEntry : std::filesystem::recursive_directory_iterator(
			GProjectContentDirectory,
			std::filesystem::directory_options::skip_permission_denied,
			ErrorCode))
		{
			if (DirectoryEntry.is_directory())
			{
				DesiredPaths.insert(FsPathToQString(DirectoryEntry.path()));
			}
		}
	}

	const QStringList CurrentPaths = m_content_disk_watcher_->directories();
	for (const QString& Path : CurrentPaths)
	{
		if (!DesiredPaths.contains(Path))
		{
			m_content_disk_watcher_->removePath(Path);
		}
	}

	for (const QString& Path : DesiredPaths)
	{
		if (!m_content_disk_watcher_->directories().contains(Path))
		{
			m_content_disk_watcher_->addPath(Path);
		}
	}
}

void AssetBrowserPanelWidget::MaybePollContentDiskChanges()
{
	const qint64 NowMs = QDateTime::currentMSecsSinceEpoch();
	if (NowMs - m_last_content_disk_poll_ms_ < kContentDiskPollIntervalMs)
	{
		return;
	}

	m_last_content_disk_poll_ms_ = NowMs;
	ApplyContentDiskChanges();
}

void AssetBrowserPanelWidget::RefreshFolderTreeFromDisk()
{
	RebuildFolderTree();
	ApplyFilters();
}

void AssetBrowserPanelWidget::MaybeUpdateContentDiskWatchPaths()
{
	const uint32_t DirectoryCount = CountContentDirectories();
	if (DirectoryCount == m_watched_directory_count_)
	{
		return;
	}

	m_watched_directory_count_ = DirectoryCount;
	UpdateContentDiskWatchPaths();
}

void AssetBrowserPanelWidget::ApplyContentDiskChanges()
{
	const uint64_t UAssetFingerprint = ComputeUAssetDiskFingerprint();
	const uint64_t DirectoryFingerprint = ComputeDirectoryDiskFingerprint();
	const bool bUAssetChanged = UAssetFingerprint != m_uasset_disk_fingerprint_;
	const bool bDirectoryChanged = DirectoryFingerprint != m_directory_disk_fingerprint_;
	if (!bUAssetChanged && !bDirectoryChanged)
	{
		return;
	}

	if (bUAssetChanged)
	{
		m_uasset_disk_fingerprint_ = UAssetFingerprint;
		FAssetRegistry::Get().ScanContentDirectory();
		m_last_registry_revision_ = 0;
	}

	if (bDirectoryChanged)
	{
		m_directory_disk_fingerprint_ = DirectoryFingerprint;
		MaybeUpdateContentDiskWatchPaths();
	}

	if (bUAssetChanged)
	{
		SyncUiFromRegistry();
		return;
	}

	RefreshFolderTreeFromDisk();
}

void AssetBrowserPanelWidget::OnDebouncedContentDiskRescan()
{
	ApplyContentDiskChanges();
}

void AssetBrowserPanelWidget::RefreshFromRegistry()
{
	MaybePollContentDiskChanges();
	SyncUiFromRegistry();
}

void AssetBrowserPanelWidget::SyncUiFromRegistry()
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
		auto* TopItem = new QTreeWidgetItem(m_folder_tree_, {Utf8StdStringToQString(Child->DisplayName)});
		TopItem->setData(0, Qt::UserRole, Utf8StdStringToQString(Child->Path));
		const bool bCanRenameFolder =
			Child->Path != "All" && Child->Path != "Content";
		if (bCanRenameFolder)
		{
			TopItem->setFlags(TopItem->flags() | Qt::ItemIsEditable);
		}
		PopulateTreeItem(TopItem, *Child);
	}

	SelectFolderTreeItemByPath(m_selected_folder_path_);
}

void AssetBrowserPanelWidget::ApplyFilters()
{
	std::vector<FAssetBrowserListItem> FilteredItems;
	FilteredItems.reserve(m_all_entries_.size());

	const bool bUseNoneFilter = (m_type_filter_ == QStringLiteral("__none__"));
	const QString SearchLower = m_search_text_.trimmed().toLower();

	if (bUseNoneFilter)
	{
		if (QTreeWidgetItem* FolderItem = FindFolderTreeItemByPath(m_selected_folder_path_))
		{
			for (int ChildIndex = 0; ChildIndex < FolderItem->childCount(); ++ChildIndex)
			{
				QTreeWidgetItem* ChildItem = FolderItem->child(ChildIndex);
				if (ChildItem == nullptr)
				{
					continue;
				}

				const QString FolderName = ChildItem->text(0);
				const QString FolderPath = ChildItem->data(0, Qt::UserRole).toString();
				if (!SearchLower.isEmpty() && !FolderName.toLower().contains(SearchLower)
					&& !FolderPath.toLower().contains(SearchLower))
				{
					continue;
				}

				FAssetBrowserListItem FolderListItem;
				FolderListItem.bIsFolder = true;
				FolderListItem.FolderPath = FolderPath.toStdString();
				FolderListItem.Entry.ObjectName = FolderName.toStdString();
				FolderListItem.Entry.AssetPath = FolderListItem.FolderPath;
				FilteredItems.push_back(std::move(FolderListItem));
			}
		}
	}

	for (const FAssetRegistryEntry& Entry : m_all_entries_)
	{
		if (bUseNoneFilter)
		{
			if (!AssetFolderTreeBuilder::IsAssetDirectChildOfFolder(Entry, m_selected_folder_path_))
			{
				continue;
			}
		}
		else if (!AssetFolderTreeBuilder::IsAssetInFolder(Entry, m_selected_folder_path_))
		{
			continue;
		}

		if (!bUseNoneFilter && !m_type_filter_.isEmpty())
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
			if (A.bIsFolder != B.bIsFolder)
			{
				return A.bIsFolder > B.bIsFolder;
			}
			return A.Entry.ObjectName < B.Entry.ObjectName;
		});

	m_list_model_->SetItems(std::move(FilteredItems));
	m_asset_grid_->clearSelection();
	m_asset_grid_->setCurrentIndex(QModelIndex());
	m_asset_grid_->viewport()->setProperty("assetDragSourceRow", kAssetDragSourceNone);
	m_status_label_->setText(tr("%1 items").arg(m_list_model_->rowCount()));

	for (int Row = 0; Row < m_list_model_->rowCount(); ++Row)
	{
		const FAssetBrowserListItem* Item = m_list_model_->GetItemAt(Row);
		if (Item != nullptr && !Item->bIsFolder)
		{
			RequestThumbnailForRow(Row, Row == 0);
		}
	}
}

void AssetBrowserPanelWidget::OnAssetGridDoubleClicked(const QModelIndex& InIndex)
{
	if (!InIndex.isValid())
	{
		return;
	}

	const FAssetBrowserListItem* Item = m_list_model_->GetItemAt(InIndex.row());
	if (Item == nullptr || !Item->bIsFolder || Item->FolderPath.empty())
	{
		return;
	}

	SelectFolderTreeItemByPath(Item->FolderPath);
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
	if (Item == nullptr || Item->bIsFolder || Item->bThumbnailPending || !Item->Thumbnail.isNull())
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

	const bool bHasSearchText = !InText.trimmed().isEmpty();
	if (bHasSearchText && m_b_can_auto_switch_to_all_types_on_search_)
	{
		m_b_can_auto_switch_to_all_types_on_search_ = false;
		m_b_can_auto_switch_to_none_on_search_clear_ = true;

		const int AllTypesIndex = m_type_filter_combo_->findData(QString());
		if (AllTypesIndex >= 0)
		{
			m_type_filter_combo_->blockSignals(true);
			m_type_filter_combo_->setCurrentIndex(AllTypesIndex);
			m_type_filter_combo_->blockSignals(false);
			m_type_filter_ = QString();
		}
	}
	else if (!bHasSearchText && m_b_can_auto_switch_to_none_on_search_clear_)
	{
		m_b_can_auto_switch_to_none_on_search_clear_ = false;
		m_b_can_auto_switch_to_all_types_on_search_ = true;

		const int NoneIndex = m_type_filter_combo_->findData(QStringLiteral("__none__"));
		if (NoneIndex >= 0)
		{
			m_type_filter_combo_->blockSignals(true);
			m_type_filter_combo_->setCurrentIndex(NoneIndex);
			m_type_filter_combo_->blockSignals(false);
			m_type_filter_ = QStringLiteral("__none__");
		}
	}

	ApplyFilters();
}

void AssetBrowserPanelWidget::OnTypeFilterChanged(int InIndex)
{
	m_type_filter_ = m_type_filter_combo_->itemData(InIndex).toString();
	ApplyFilters();
}

void AssetBrowserPanelWidget::OnFolderTreeContextMenuRequested(const QPoint& InPos)
{
	QTreeWidgetItem* Item = m_folder_tree_->itemAt(InPos);
	if (Item != nullptr)
	{
		m_folder_tree_->setCurrentItem(Item);
	}
	else
	{
		Item = m_folder_tree_->currentItem();
	}

	if (Item == nullptr)
	{
		return;
	}

	QMenu Menu(this);
	QAction* AddFolderAction = Menu.addAction(tr("添加新文件夹"));
	AddFolderAction->setEnabled(CanCreateFolderUnderItem(Item));
	QAction* RenameAction = Menu.addAction(tr("重命名"));
	RenameAction->setShortcut(QKeySequence(Qt::Key_F2));
	RenameAction->setEnabled(CanRenameFolderItem(Item));

	QAction* Chosen = Menu.exec(m_folder_tree_->viewport()->mapToGlobal(InPos));
	if (Chosen == AddFolderAction)
	{
		OnAddNewFolderRequested();
	}
	else if (Chosen == RenameAction)
	{
		BeginRenameSelectedFolder();
	}
}

void AssetBrowserPanelWidget::OnAddNewFolderRequested()
{
	QTreeWidgetItem* CurrentItem = m_folder_tree_->currentItem();
	if (!CanCreateFolderUnderItem(CurrentItem))
	{
		return;
	}

	std::string ParentFolderPath = CurrentItem->data(0, Qt::UserRole).toString().toStdString();
	if (ParentFolderPath == "All")
	{
		ParentFolderPath = "Content";
	}

	const QString NewName = QInputDialog::getText(
		this,
		tr("添加新文件夹"),
		tr("文件夹名称:"),
		QLineEdit::Normal,
		tr("NewFolder"));
	if (NewName.trimmed().isEmpty())
	{
		return;
	}

	std::string NewFolderPath;
	std::string ErrorMessage;
	if (!FAssetRenameService::CreateFolder(
		ParentFolderPath,
		NewName.trimmed().toStdString(),
		&NewFolderPath,
		&ErrorMessage))
	{
		QMessageBox::warning(
			this,
			tr("创建失败"),
			tr("无法创建文件夹: %1").arg(QString::fromStdString(ErrorMessage)));
		return;
	}

	m_selected_folder_path_ = NewFolderPath;
	m_last_registry_revision_ = 0;
	m_uasset_disk_fingerprint_ = ComputeUAssetDiskFingerprint();
	m_directory_disk_fingerprint_ = ComputeDirectoryDiskFingerprint();
	MaybeUpdateContentDiskWatchPaths();
	RefreshFromRegistry();
}

void AssetBrowserPanelWidget::OnGridContextMenuRequested(const QPoint& InPos)
{
	const QModelIndex Index = m_asset_grid_->indexAt(InPos);
	if (!Index.isValid())
	{
		return;
	}

	m_asset_grid_->setCurrentIndex(Index);

	if (Index.data(static_cast<int>(EAssetListRole::IsFolder)).toBool())
	{
		return;
	}

	const QString SoftPath = Index.data(static_cast<int>(EAssetListRole::SoftObjectPath)).toString();
	const QString UAssetPath = Index.data(static_cast<int>(EAssetListRole::UAssetFilePath)).toString();
	const QString AssetType = Index.data(static_cast<int>(EAssetListRole::AssetType)).toString();

	QMenu Menu(this);
	QAction* ShowInExplorerAction = Menu.addAction(tr("在资源管理器中显示"));
	QAction* CopyPathAction = Menu.addAction(tr("复制 SoftObjectPath"));
	QAction* DuplicateAction = Menu.addAction(tr("创建副本"));
	QAction* RenameAction = Menu.addAction(tr("重命名"));
	RenameAction->setShortcut(QKeySequence(Qt::Key_F2));
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
	else if (Chosen == DuplicateAction)
	{
		DuplicateSelectedAsset();
	}
	else if (Chosen == RenameAction)
	{
		BeginRenameSelectedAsset();
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
