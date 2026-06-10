#include "ui/AssetBrowserPanelWidget.h"

#include <algorithm>
#include <filesystem>
#include <functional>

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QAbstractItemView>
#include <QClipboard>
#include <QDesktopServices>
#include <QCursor>
#include <QDateTime>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
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
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QItemSelectionModel>
#include <QRubberBand>
#include <QScrollBar>
#include <QShowEvent>
#include <QSet>
#include <QSplitter>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "app/GameApp.h"
#include "world/Level.h"
#include "asset/AssetFolderTree.h"
#include "asset/AssetRegistry.h"
#include "asset/AssetDeleteService.h"
#include "asset/AssetDuplicateService.h"
#include "asset/AssetRenameService.h"
#include "asset/AssetTypeInfo.h"
#include "asset/MeshImportFactory.h"
#include "asset/ProjectPaths.h"
#include "asset/SoftObjectPath.h"
#include "asset/thumbnail/AssetThumbnailService.h"
#include "ui/AssetBrowserItemInteraction.h"
#include "ui/AssetBrowserItemTooltipWidget.h"
#include "ui/AssetBrowserTypeFilterWidget.h"
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

QString FolderPathToDisplayName(const std::string& InFolderPath)
{
	if (InFolderPath.empty() || InFolderPath == "All")
	{
		return QStringLiteral("All");
	}

	const size_t LastSlashIndex = InFolderPath.find_last_of('/');
	if (LastSlashIndex == std::string::npos)
	{
		return Utf8StdStringToQString(InFolderPath);
	}

	return Utf8StdStringToQString(InFolderPath.substr(LastSlashIndex + 1));
}

bool IsSubfolderPath(const std::string& InChildPath, const std::string& InParentPath)
{
	if (InParentPath.empty() || InChildPath.empty() || InChildPath == InParentPath)
	{
		return false;
	}

	const std::string Prefix = InParentPath + "/";
	return InChildPath.size() > Prefix.size() && InChildPath.compare(0, Prefix.size(), Prefix) == 0;
}

std::vector<std::string> DeduplicateNestedFolderPaths(std::vector<std::string> InFolderPaths)
{
	std::sort(
		InFolderPaths.begin(),
		InFolderPaths.end(),
		[](const std::string& A, const std::string& B)
		{
			if (A.size() != B.size())
			{
				return A.size() < B.size();
			}
			return A < B;
		});

	std::vector<std::string> RootFolderPaths;
	for (const std::string& FolderPath : InFolderPaths)
	{
		bool bIsNestedSelection = false;
		for (const std::string& RootPath : RootFolderPaths)
		{
			if (IsSubfolderPath(FolderPath, RootPath))
			{
				bIsNestedSelection = true;
				break;
			}
		}

		if (!bIsNestedSelection)
		{
			RootFolderPaths.push_back(FolderPath);
		}
	}

	return RootFolderPaths;
}

bool FolderFilterIncludesAll(const std::vector<std::string>& InFolderPaths)
{
	return std::find(InFolderPaths.begin(), InFolderPaths.end(), "All") != InFolderPaths.end();
}

void NormalizeSelectedFolderPaths(std::vector<std::string>& InOutFolderPaths)
{
	if (InOutFolderPaths.empty())
	{
		InOutFolderPaths.push_back("All");
		return;
	}

	if (FolderFilterIncludesAll(InOutFolderPaths))
	{
		InOutFolderPaths = {"All"};
		return;
	}

	InOutFolderPaths = DeduplicateNestedFolderPaths(std::move(InOutFolderPaths));
}

bool IsAssetInAnySelectedFolder(
	const FAssetRegistryEntry& InEntry,
	const std::vector<std::string>& InFolderPaths)
{
	for (const std::string& FolderPath : InFolderPaths)
	{
		if (AssetFolderTreeBuilder::IsAssetInFolder(InEntry, FolderPath))
		{
			return true;
		}
	}

	return false;
}

bool IsAssetDirectChildOfAnySelectedFolder(
	const FAssetRegistryEntry& InEntry,
	const std::vector<std::string>& InFolderPaths)
{
	for (const std::string& FolderPath : InFolderPaths)
	{
		if (AssetFolderTreeBuilder::IsAssetDirectChildOfFolder(InEntry, FolderPath))
		{
			return true;
		}
	}

	return false;
}

bool IsDraggableFolderPath(const std::string& InFolderPath)
{
	return !InFolderPath.empty() && InFolderPath != "All" && InFolderPath != "Content";
}

bool CanMoveFolderInto(const std::string& InSourceFolderPath, const std::string& InTargetFolderPath)
{
	if (!IsDraggableFolderPath(InSourceFolderPath))
	{
		return false;
	}

	if (InTargetFolderPath.empty() || InTargetFolderPath == "All")
	{
		return false;
	}

	if (InSourceFolderPath == InTargetFolderPath)
	{
		return false;
	}

	if (IsSubfolderPath(InTargetFolderPath, InSourceFolderPath))
	{
		return false;
	}

	return true;
}

bool CanDropFoldersOnTarget(
	const std::vector<std::string>& InSourceFolderPaths,
	const std::string& InTargetFolderPath)
{
	const std::vector<std::string> RootFolderPaths = DeduplicateNestedFolderPaths(InSourceFolderPaths);
	if (RootFolderPaths.empty())
	{
		return false;
	}

	for (const std::string& SourceFolderPath : RootFolderPaths)
	{
		if (!CanMoveFolderInto(SourceFolderPath, InTargetFolderPath))
		{
			return false;
		}
	}

	return true;
}

QPixmap BuildFolderDragPixmap(int InItemCount)
{
	constexpr int kDragPixmapSize = 48;
	QPixmap DragPixmap(kDragPixmapSize, kDragPixmapSize);
	DragPixmap.fill(Qt::transparent);
	{
		QPainter PixmapPainter(&DragPixmap);
		PixmapPainter.setRenderHint(QPainter::Antialiasing, true);
		PixmapPainter.setPen(QPen(QColor("#c9a227"), 2));
		PixmapPainter.setBrush(QColor("#5c4a12"));
		PixmapPainter.drawRoundedRect(DragPixmap.rect().adjusted(1, 1, -1, -1), 4, 4);
		PixmapPainter.setPen(QColor("#f0f0f0"));
		PixmapPainter.drawText(DragPixmap.rect().adjusted(4, 4, -4, -4), Qt::AlignCenter, QObject::tr("Folder"));

		if (InItemCount > 1)
		{
			PixmapPainter.setPen(Qt::white);
			QFont CountFont = PixmapPainter.font();
			CountFont.setBold(true);
			CountFont.setPointSize(9);
			PixmapPainter.setFont(CountFont);
			PixmapPainter.drawText(
				DragPixmap.rect().adjusted(2, 2, -2, -2),
				Qt::AlignRight | Qt::AlignTop,
				QString::number(InItemCount));
		}
	}
	return DragPixmap;
}

QPoint GetMouseEventGlobalPos(const QMouseEvent* InEvent)
{
	if (InEvent == nullptr)
	{
		return {};
	}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return InEvent->globalPosition().toPoint();
#else
	return InEvent->globalPos();
#endif
}

QPoint GetContextMenuEventGlobalPos(const QContextMenuEvent* InEvent)
{
	if (InEvent == nullptr)
	{
		return {};
	}

	return InEvent->globalPos();
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

bool HasImportableModelFileMimeData(const QMimeData* InMimeData)
{
	if (InMimeData == nullptr || !InMimeData->hasUrls())
	{
		return false;
	}

	std::vector<std::filesystem::path> ImportablePaths;
	return ExtractImportableModelFiles(InMimeData->urls(), &ImportablePaths);
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
		setDragEnabled(true);
		setDragDropMode(QAbstractItemView::DragDrop);
		setDefaultDropAction(Qt::CopyAction);
		setSelectionMode(QAbstractItemView::ExtendedSelection);
		setSelectionBehavior(QAbstractItemView::SelectRows);
		setFocusPolicy(Qt::StrongFocus);
		viewport()->installEventFilter(this);
	}

	std::function<void(
		const std::string& InTargetFolderPath,
		const std::vector<std::string>& InSoftObjectPaths,
		const std::vector<std::string>& InFolderPaths)> ItemDropHandler;
	std::function<void()> DeleteRequestedHandler;

protected:
	void CollectItemsInRect(const QRect& InViewportRect, QList<QTreeWidgetItem*>* OutItems) const
	{
		if (OutItems == nullptr || InViewportRect.isEmpty())
		{
			return;
		}

		std::function<void(QTreeWidgetItem*)> VisitItem;
		VisitItem = [&](QTreeWidgetItem* InItem)
		{
			if (InItem == nullptr)
			{
				return;
			}

			if (visualItemRect(InItem).intersects(InViewportRect))
			{
				OutItems->push_back(InItem);
			}

			for (int ChildIndex = 0; ChildIndex < InItem->childCount(); ++ChildIndex)
			{
				VisitItem(InItem->child(ChildIndex));
			}
		};

		for (int TopIndex = 0; TopIndex < topLevelItemCount(); ++TopIndex)
		{
			VisitItem(topLevelItem(TopIndex));
		}
	}

	void ApplyRubberBandSelection(const QRect& InBandRect, Qt::KeyboardModifiers InModifiers)
	{
		constexpr int kClickThresholdPx = 4;
		if (InBandRect.width() < kClickThresholdPx && InBandRect.height() < kClickThresholdPx)
		{
			if ((InModifiers & Qt::ControlModifier) == 0)
			{
				clearSelection();
				setCurrentItem(nullptr);
			}
			return;
		}

		QList<QTreeWidgetItem*> MatchedItems;
		CollectItemsInRect(InBandRect, &MatchedItems);
		if (MatchedItems.isEmpty())
		{
			if ((InModifiers & Qt::ControlModifier) == 0)
			{
				clearSelection();
				setCurrentItem(nullptr);
			}
			return;
		}

		QItemSelection NewSelection;
		for (QTreeWidgetItem* Item : MatchedItems)
		{
			if (Item == nullptr)
			{
				continue;
			}

			const QModelIndex Index = indexFromItem(Item);
			if (Index.isValid())
			{
				NewSelection.select(Index, Index);
			}
		}

		QItemSelectionModel::SelectionFlags SelectionFlags = QItemSelectionModel::Select;
		if ((InModifiers & Qt::ControlModifier) == 0)
		{
			SelectionFlags |= QItemSelectionModel::Clear;
		}

		selectionModel()->select(NewSelection, SelectionFlags);
		if (!MatchedItems.isEmpty())
		{
			setCurrentItem(MatchedItems.last());
		}
		viewport()->setFocus(Qt::MouseFocusReason);
	}

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

	bool IsValidItemDropTarget(const QTreeWidgetItem* InItem, const QMimeData* InMimeData) const
	{
		if (!IsValidAssetDropTargetItem(InItem) || InMimeData == nullptr)
		{
			return false;
		}

		const std::string TargetFolderPath = InItem->data(0, Qt::UserRole).toString().toStdString();
		if (InMimeData->hasFormat(kFolderPathMimeType))
		{
			const std::vector<std::string> SourceFolderPaths = ExtractFolderPathsFromMimeData(InMimeData);
			if (!CanDropFoldersOnTarget(SourceFolderPaths, TargetFolderPath))
			{
				return false;
			}
		}

		return true;
	}

	void dragEnterEvent(QDragEnterEvent* InEvent) override
	{
		if (HasContentBrowserDropMimeData(InEvent->mimeData()))
		{
			QTreeWidgetItem* TargetItem = itemAt(InEvent->position().toPoint());
			if (IsValidItemDropTarget(TargetItem, InEvent->mimeData()))
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

		QTreeWidget::dragEnterEvent(InEvent);
	}

	void dragMoveEvent(QDragMoveEvent* InEvent) override
	{
		if (HasContentBrowserDropMimeData(InEvent->mimeData()))
		{
			QTreeWidgetItem* TargetItem = itemAt(InEvent->position().toPoint());
			if (IsValidItemDropTarget(TargetItem, InEvent->mimeData()))
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

		if (!HasContentBrowserDropMimeData(InEvent->mimeData()))
		{
			QTreeWidget::dropEvent(InEvent);
			return;
		}

		QTreeWidgetItem* TargetItem = itemAt(InEvent->position().toPoint());
		if (!IsValidItemDropTarget(TargetItem, InEvent->mimeData()))
		{
			InEvent->ignore();
			return;
		}

		const std::string TargetFolderPath = TargetItem->data(0, Qt::UserRole).toString().toStdString();
		const std::vector<std::string> SoftPaths = ExtractSoftObjectPathsFromMimeData(InEvent->mimeData());
		const std::vector<std::string> FolderPaths = ExtractFolderPathsFromMimeData(InEvent->mimeData());
		if (ItemDropHandler && (!SoftPaths.empty() || !FolderPaths.empty()))
		{
			ItemDropHandler(TargetFolderPath, SoftPaths, FolderPaths);
		}

		InEvent->setDropAction(Qt::CopyAction);
		InEvent->accept();
	}

	void startDrag(Qt::DropActions InSupportedActions) override
	{
		QList<QTreeWidgetItem*> DragItems = selectedItems();
		if (DragItems.isEmpty())
		{
			QTreeWidgetItem* CurrentItem = currentItem();
			if (CurrentItem == nullptr)
			{
				return;
			}

			DragItems = {CurrentItem};
		}

		QStringList FolderPaths;
		for (QTreeWidgetItem* Item : DragItems)
		{
			if (Item == nullptr)
			{
				continue;
			}

			const std::string FolderPath = Item->data(0, Qt::UserRole).toString().toStdString();
			if (!IsDraggableFolderPath(FolderPath))
			{
				continue;
			}

			const QString QFolderPath = QString::fromStdString(FolderPath);
			if (!FolderPaths.contains(QFolderPath))
			{
				FolderPaths.push_back(QFolderPath);
			}
		}

		if (FolderPaths.isEmpty())
		{
			return;
		}

		auto* MimeData = new QMimeData();
		MimeData->setData(kFolderPathMimeType, FolderPaths.join('\n').toUtf8());

		auto* Drag = new QDrag(this);
		Drag->setMimeData(MimeData);
		Drag->setPixmap(BuildFolderDragPixmap(FolderPaths.size()));
		Drag->setHotSpot(QPoint(24, 24));
		Drag->exec(Qt::CopyAction, Qt::CopyAction);
	}

	bool eventFilter(QObject* InWatched, QEvent* InEvent) override
	{
		if (InWatched != viewport())
		{
			return QTreeWidget::eventFilter(InWatched, InEvent);
		}

		if (InEvent->type() == QEvent::MouseButtonPress)
		{
			auto* MouseEvent = static_cast<QMouseEvent*>(InEvent);
			if (MouseEvent->button() == Qt::RightButton)
			{
				const QPoint ViewportPos = MouseEvent->pos();
				QTreeWidgetItem* Item = itemAt(ViewportPos);
				if (Item != nullptr && selectedItems().contains(Item))
				{
					setCurrentItem(Item);
					MouseEvent->accept();
					return true;
				}
			}
		}

		return QTreeWidget::eventFilter(InWatched, InEvent);
	}

	void contextMenuEvent(QContextMenuEvent* InEvent) override
	{
		const QPoint ViewportPos = viewport()->mapFromGlobal(GetContextMenuEventGlobalPos(InEvent));
		QTreeWidgetItem* Item = itemAt(ViewportPos);
		if (Item != nullptr)
		{
			if (!selectedItems().contains(Item))
			{
				clearSelection();
				setCurrentItem(Item);
			}
			else
			{
				setCurrentItem(Item);
			}
		}

		emit customContextMenuRequested(ViewportPos);
		InEvent->accept();
	}

	void keyPressEvent(QKeyEvent* InEvent) override
	{
		if (InEvent != nullptr && InEvent->key() == Qt::Key_Delete && DeleteRequestedHandler)
		{
			DeleteRequestedHandler();
			InEvent->accept();
			return;
		}

		QTreeWidget::keyPressEvent(InEvent);
	}

	void mousePressEvent(QMouseEvent* InEvent) override
	{
		if (InEvent->button() == Qt::LeftButton)
		{
			const QPoint ViewportPos = viewport()->mapFromGlobal(GetMouseEventGlobalPos(InEvent));
			if (itemAt(ViewportPos) == nullptr)
			{
				viewport()->setFocus(Qt::MouseFocusReason);
				m_b_rubber_band_selecting_ = true;
				m_rubber_band_origin_ = ViewportPos;
				if (m_rubber_band_ == nullptr)
				{
					m_rubber_band_ = new QRubberBand(QRubberBand::Rectangle, viewport());
				}
				m_rubber_band_->setGeometry(QRect(m_rubber_band_origin_, QSize()));
				m_rubber_band_->show();
				InEvent->accept();
				return;
			}
		}

		QTreeWidget::mousePressEvent(InEvent);
	}

	void mouseMoveEvent(QMouseEvent* InEvent) override
	{
		if (m_b_rubber_band_selecting_ && m_rubber_band_ != nullptr)
		{
			const QPoint ViewportPos = viewport()->mapFromGlobal(GetMouseEventGlobalPos(InEvent));
			m_rubber_band_->setGeometry(QRect(m_rubber_band_origin_, ViewportPos).normalized());
			InEvent->accept();
			return;
		}

		QTreeWidget::mouseMoveEvent(InEvent);
	}

	void mouseReleaseEvent(QMouseEvent* InEvent) override
	{
		if (m_b_rubber_band_selecting_ && InEvent->button() == Qt::LeftButton)
		{
			const QRect BandRect = m_rubber_band_ != nullptr ? m_rubber_band_->geometry() : QRect();
			if (m_rubber_band_ != nullptr)
			{
				m_rubber_band_->hide();
			}
			m_b_rubber_band_selecting_ = false;
			ApplyRubberBandSelection(BandRect, InEvent->modifiers());
			InEvent->accept();
			return;
		}

		QTreeWidget::mouseReleaseEvent(InEvent);
	}

private:
	QTreeWidgetItem* m_asset_drop_hover_item_ = nullptr;
	bool m_b_rubber_band_selecting_ = false;
	QPoint m_rubber_band_origin_;
	QRubberBand* m_rubber_band_ = nullptr;
};

class AssetBrowserListView final : public QListView
{
public:
	explicit AssetBrowserListView(QWidget* InParent = nullptr)
		: QListView(InParent)
	{
		setDropIndicatorShown(false);
		setMouseTracking(true);
		viewport()->setMouseTracking(true);
		viewport()->setProperty("folderDropHoverRow", kAssetDragSourceNone);
		SetItemHoverRow(kAssetDragSourceNone);
		viewport()->installEventFilter(this);

		m_tooltip_show_timer_ = new QTimer(this);
		m_tooltip_show_timer_->setSingleShot(true);
		m_tooltip_show_timer_->setInterval(350);
		connect(m_tooltip_show_timer_, &QTimer::timeout, this, [this]()
		{
			ShowPendingItemTooltip();
		});
	}

	void DismissItemHover()
	{
		ClearItemHoverState();
	}

	std::function<void(const QList<QUrl>&)> ExternalDropHandler;
	std::function<void(
		const std::string& InTargetFolderPath,
		const std::vector<std::string>& InSoftObjectPaths,
		const std::vector<std::string>& InFolderPaths)> ItemDropHandler;
	std::function<void()> DeleteRequestedHandler;
	std::function<void(int InWheelDeltaY)> TileSizeWheelHandler;

protected:
	QModelIndexList CollectIndexesInRect(const QRect& InViewportRect) const
	{
		QModelIndexList MatchedIndexes;
		if (model() == nullptr || InViewportRect.isEmpty())
		{
			return MatchedIndexes;
		}

		for (int Row = 0; Row < model()->rowCount(); ++Row)
		{
			const QModelIndex RowIndex = model()->index(Row, 0);
			if (RowIndex.isValid() && visualRect(RowIndex).intersects(InViewportRect))
			{
				MatchedIndexes.push_back(RowIndex);
			}
		}

		return MatchedIndexes;
	}

	void ApplyRubberBandSelection(const QRect& InBandRect, Qt::KeyboardModifiers InModifiers)
	{
		constexpr int kClickThresholdPx = 4;
		if (InBandRect.width() < kClickThresholdPx && InBandRect.height() < kClickThresholdPx)
		{
			if ((InModifiers & Qt::ControlModifier) == 0)
			{
				clearSelection();
				setCurrentIndex(QModelIndex());
			}
			return;
		}

		const QModelIndexList MatchedIndexes = CollectIndexesInRect(InBandRect);
		if (MatchedIndexes.isEmpty())
		{
			if ((InModifiers & Qt::ControlModifier) == 0)
			{
				clearSelection();
				setCurrentIndex(QModelIndex());
			}
			return;
		}

		QItemSelection NewSelection;
		for (const QModelIndex& Index : MatchedIndexes)
		{
			NewSelection.select(Index, Index);
		}

		QItemSelectionModel::SelectionFlags SelectionFlags = QItemSelectionModel::Select;
		if ((InModifiers & Qt::ControlModifier) == 0)
		{
			SelectionFlags |= QItemSelectionModel::Clear;
		}

		selectionModel()->select(NewSelection, SelectionFlags);
		selectionModel()->setCurrentIndex(
			MatchedIndexes.last(),
			QItemSelectionModel::Current);
		viewport()->setFocus(Qt::MouseFocusReason);
	}

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

	bool IsValidItemDropTargetAt(const QPoint& InViewportPos, const QMimeData* InMimeData) const
	{
		const QModelIndex Index = FindFolderIndexAt(InViewportPos);
		if (!Index.isValid() || InMimeData == nullptr)
		{
			return false;
		}

		const std::string TargetFolderPath =
			Index.data(static_cast<int>(EAssetListRole::FolderPath)).toString().toStdString();
		if (TargetFolderPath.empty())
		{
			return false;
		}

		if (InMimeData->hasFormat(kFolderPathMimeType))
		{
			const std::vector<std::string> SourceFolderPaths = ExtractFolderPathsFromMimeData(InMimeData);
			if (!CanDropFoldersOnTarget(SourceFolderPaths, TargetFolderPath))
			{
				return false;
			}
		}

		return true;
	}

	void ClearFolderDropHoverRow()
	{
		if (viewport()->property("folderDropHoverRow").toInt() == kAssetDragSourceNone)
		{
			return;
		}

		viewport()->setProperty("folderDropHoverRow", kAssetDragSourceNone);
		viewport()->update();
	}

	void UpdateFolderDropHoverAt(const QPoint& InViewportPos)
	{
		const QModelIndex Index = FindFolderIndexAt(InViewportPos);
		const int HoverRow = Index.isValid() ? Index.row() : kAssetDragSourceNone;
		if (viewport()->property("folderDropHoverRow").toInt() == HoverRow)
		{
			return;
		}

		viewport()->setProperty("folderDropHoverRow", HoverRow);
		viewport()->update();
	}

	void AcceptItemMoveDrop(QDropEvent* InEvent, const QPoint& InViewportPos)
	{
		if (!IsValidItemDropTargetAt(InViewportPos, InEvent->mimeData()))
		{
			return;
		}

		const QModelIndex Index = FindFolderIndexAt(InViewportPos);
		const std::string TargetFolderPath =
			Index.data(static_cast<int>(EAssetListRole::FolderPath)).toString().toStdString();
		const std::vector<std::string> SoftPaths = ExtractSoftObjectPathsFromMimeData(InEvent->mimeData());
		const std::vector<std::string> FolderPaths = ExtractFolderPathsFromMimeData(InEvent->mimeData());
		if (!TargetFolderPath.empty() && ItemDropHandler && (!SoftPaths.empty() || !FolderPaths.empty()))
		{
			ItemDropHandler(TargetFolderPath, SoftPaths, FolderPaths);
		}

		InEvent->setDropAction(Qt::CopyAction);
		InEvent->accept();
	}

	void HandleContentBrowserDragEnter(QDragEnterEvent* InEvent)
	{
		const QPoint ViewportPos = InEvent->position().toPoint();
		if (IsValidItemDropTargetAt(ViewportPos, InEvent->mimeData()))
		{
			InEvent->setDropAction(Qt::CopyAction);
			InEvent->accept();
			UpdateFolderDropHoverAt(ViewportPos);
			return;
		}

		ClearFolderDropHoverRow();
		InEvent->accept();
	}

	void HandleContentBrowserDragMove(QDragMoveEvent* InEvent)
	{
		const QPoint ViewportPos = InEvent->position().toPoint();
		if (IsValidItemDropTargetAt(ViewportPos, InEvent->mimeData()))
		{
			InEvent->setDropAction(Qt::CopyAction);
			InEvent->accept();
			UpdateFolderDropHoverAt(ViewportPos);
			return;
		}

		ClearFolderDropHoverRow();
		InEvent->ignore();
	}

	void HandleContentBrowserDrop(QDropEvent* InEvent)
	{
		ClearFolderDropHoverRow();
		const QPoint ViewportPos = InEvent->position().toPoint();
		if (IsValidItemDropTargetAt(ViewportPos, InEvent->mimeData()))
		{
			AcceptItemMoveDrop(InEvent, ViewportPos);
			return;
		}

		InEvent->ignore();
	}

	void dragEnterEvent(QDragEnterEvent* InEvent) override
	{
		if (HasImportableModelFileMimeData(InEvent->mimeData()))
		{
			InEvent->acceptProposedAction();
			return;
		}

		if (HasContentBrowserDropMimeData(InEvent->mimeData()))
		{
			HandleContentBrowserDragEnter(InEvent);
			return;
		}

		QListView::dragEnterEvent(InEvent);
	}

	void dragMoveEvent(QDragMoveEvent* InEvent) override
	{
		if (HasImportableModelFileMimeData(InEvent->mimeData()))
		{
			InEvent->acceptProposedAction();
			return;
		}

		if (HasContentBrowserDropMimeData(InEvent->mimeData()))
		{
			HandleContentBrowserDragMove(InEvent);
			return;
		}

		QListView::dragMoveEvent(InEvent);
	}

	void dragLeaveEvent(QDragLeaveEvent* InEvent) override
	{
		ClearFolderDropHoverRow();
		DismissItemHover();
		QListView::dragLeaveEvent(InEvent);
	}

	void dropEvent(QDropEvent* InEvent) override
	{
		if (HasImportableModelFileMimeData(InEvent->mimeData()))
		{
			if (ExternalDropHandler)
			{
				ExternalDropHandler(InEvent->mimeData()->urls());
			}
			InEvent->acceptProposedAction();
			return;
		}

		if (HasContentBrowserDropMimeData(InEvent->mimeData()))
		{
			HandleContentBrowserDrop(InEvent);
			return;
		}

		QListView::dropEvent(InEvent);
	}

	bool eventFilter(QObject* InWatched, QEvent* InEvent) override
	{
		if (InWatched != viewport())
		{
			return QListView::eventFilter(InWatched, InEvent);
		}

		if (InEvent->type() == QEvent::DragEnter)
		{
			return false;
		}

		if (InEvent->type() == QEvent::DragMove)
		{
			return false;
		}

		if (InEvent->type() == QEvent::Drop)
		{
			return false;
		}

		if (InEvent->type() == QEvent::Leave)
		{
			DismissItemHover();
			return false;
		}

		if (InEvent->type() == QEvent::Wheel)
		{
			auto* WheelEvent = static_cast<QWheelEvent*>(InEvent);
			if (WheelEvent != nullptr
				&& (WheelEvent->modifiers() & Qt::ControlModifier) != 0
				&& TileSizeWheelHandler)
			{
				HideItemTooltip();
				m_tooltip_show_timer_->stop();
				TileSizeWheelHandler(WheelEvent->angleDelta().y());
				WheelEvent->accept();
				return true;
			}

			HideItemTooltip();
			m_tooltip_show_timer_->stop();
			return false;
		}

		if (InEvent->type() == QEvent::MouseMove)
		{
			auto* MouseEvent = static_cast<QMouseEvent*>(InEvent);
			UpdateItemHoverAt(MouseEvent->pos());
			return false;
		}

		if (InEvent->type() == QEvent::MouseButtonPress)
		{
			auto* MouseEvent = static_cast<QMouseEvent*>(InEvent);
			if (MouseEvent->button() == Qt::RightButton)
			{
				const QPoint ViewportPos = MouseEvent->pos();
				const QModelIndex Index = indexAt(ViewportPos);
				if (Index.isValid() && selectionModel() != nullptr && selectionModel()->isSelected(Index))
				{
					selectionModel()->setCurrentIndex(Index, QItemSelectionModel::Current);
					MouseEvent->accept();
					return true;
				}
			}
		}

		return QListView::eventFilter(InWatched, InEvent);
	}

	void contextMenuEvent(QContextMenuEvent* InEvent) override
	{
		const QPoint ViewportPos = viewport()->mapFromGlobal(GetContextMenuEventGlobalPos(InEvent));
		const QModelIndex Index = indexAt(ViewportPos);
		if (Index.isValid() && selectionModel() != nullptr)
		{
			if (!selectionModel()->isSelected(Index))
			{
				selectionModel()->clearSelection();
				selectionModel()->select(
					Index,
					QItemSelectionModel::Select | QItemSelectionModel::Current);
			}
			else
			{
				selectionModel()->setCurrentIndex(Index, QItemSelectionModel::Current);
			}
		}

		emit customContextMenuRequested(ViewportPos);
		InEvent->accept();
	}

	void keyPressEvent(QKeyEvent* InEvent) override
	{
		if (InEvent != nullptr && InEvent->key() == Qt::Key_Delete)
		{
			if (DeleteRequestedHandler)
			{
				DeleteRequestedHandler();
				InEvent->accept();
				return;
			}
		}

		QListView::keyPressEvent(InEvent);
	}

	void mousePressEvent(QMouseEvent* InEvent) override
	{
		if (InEvent->button() == Qt::LeftButton)
		{
			const QPoint ViewportPos = viewport()->mapFromGlobal(GetMouseEventGlobalPos(InEvent));
			if (!indexAt(ViewportPos).isValid())
			{
				DismissItemHover();
				viewport()->setFocus(Qt::MouseFocusReason);
				m_b_rubber_band_selecting_ = true;
				m_rubber_band_origin_ = ViewportPos;
				if (m_rubber_band_ == nullptr)
				{
					m_rubber_band_ = new QRubberBand(QRubberBand::Rectangle, viewport());
				}
				m_rubber_band_->setGeometry(QRect(m_rubber_band_origin_, QSize()));
				m_rubber_band_->show();
				InEvent->accept();
				return;
			}
		}

		QListView::mousePressEvent(InEvent);
	}

	void mouseMoveEvent(QMouseEvent* InEvent) override
	{
		const QPoint ViewportPos = viewport()->mapFromGlobal(GetMouseEventGlobalPos(InEvent));
		if (m_b_rubber_band_selecting_ && m_rubber_band_ != nullptr)
		{
			m_rubber_band_->setGeometry(QRect(m_rubber_band_origin_, ViewportPos).normalized());
			InEvent->accept();
			return;
		}

		UpdateItemHoverAt(ViewportPos);
		QListView::mouseMoveEvent(InEvent);
	}

	void mouseReleaseEvent(QMouseEvent* InEvent) override
	{
		if (m_b_rubber_band_selecting_ && InEvent->button() == Qt::LeftButton)
		{
			const QRect BandRect = m_rubber_band_ != nullptr ? m_rubber_band_->geometry() : QRect();
			if (m_rubber_band_ != nullptr)
			{
				m_rubber_band_->hide();
			}
			m_b_rubber_band_selecting_ = false;
			ApplyRubberBandSelection(BandRect, InEvent->modifiers());
			InEvent->accept();
			return;
		}

		QListView::mouseReleaseEvent(InEvent);
	}

	void startDrag(Qt::DropActions InSupportedActions) override
	{
		DismissItemHover();
		const QModelIndex Current = currentIndex();
		if (!Current.isValid())
		{
			return;
		}

		QModelIndexList DragIndexes = selectedIndexes();
		if (DragIndexes.isEmpty() || !DragIndexes.contains(Current))
		{
			DragIndexes = {Current};
		}

		QStringList SoftPaths;
		QStringList FolderPaths;
		for (const QModelIndex& Index : DragIndexes)
		{
			if (!Index.isValid())
			{
				continue;
			}

			if (Index.data(static_cast<int>(EAssetListRole::IsFolder)).toBool())
			{
				const QString FolderPath = Index.data(static_cast<int>(EAssetListRole::FolderPath)).toString();
				if (!FolderPath.isEmpty() && !FolderPaths.contains(FolderPath))
				{
					FolderPaths.push_back(FolderPath);
				}
				continue;
			}

			const QString SoftPath = Index.data(static_cast<int>(EAssetListRole::SoftObjectPath)).toString();
			if (!SoftPath.isEmpty() && !SoftPaths.contains(SoftPath))
			{
				SoftPaths.push_back(SoftPath);
			}
		}

		if (SoftPaths.isEmpty() && FolderPaths.isEmpty())
		{
			return;
		}

		auto* MimeData = new QMimeData();
		if (!SoftPaths.isEmpty())
		{
			const QString EncodedSoftPaths = SoftPaths.join('\n');
			MimeData->setData(kSoftObjectPathMimeType, EncodedSoftPaths.toUtf8());
			MimeData->setText(EncodedSoftPaths);
		}
		if (!FolderPaths.isEmpty())
		{
			MimeData->setData(kFolderPathMimeType, FolderPaths.join('\n').toUtf8());
		}

		const int DragItemCount = SoftPaths.size() + FolderPaths.size();
		QPixmap DragPixmap;
		if (SoftPaths.isEmpty())
		{
			DragPixmap = BuildFolderDragPixmap(DragItemCount);
		}
		else
		{
			const QImage Thumbnail =
				Current.data(static_cast<int>(EAssetListRole::ThumbnailImage)).value<QImage>();
			constexpr int kDragPixmapSize = 48;
			DragPixmap = QPixmap(kDragPixmapSize, kDragPixmapSize);
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
					PixmapPainter.drawText(ImageRect, Qt::AlignCenter, tr("Asset"));
				}

				if (DragItemCount > 1)
				{
					PixmapPainter.setPen(Qt::white);
					QFont CountFont = PixmapPainter.font();
					CountFont.setBold(true);
					CountFont.setPointSize(9);
					PixmapPainter.setFont(CountFont);
					PixmapPainter.drawText(
						DragPixmap.rect().adjusted(2, 2, -2, -2),
						Qt::AlignRight | Qt::AlignTop,
						QString::number(DragItemCount));
				}
			}
		}

		auto* Drag = new QDrag(this);
		Drag->setMimeData(MimeData);
		Drag->setPixmap(DragPixmap);
		Drag->setHotSpot(QPoint(DragPixmap.width() / 2, DragPixmap.height() / 2));

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

private:
	void HideItemTooltip()
	{
		if (m_item_tooltip_ != nullptr)
		{
			m_item_tooltip_->hide();
		}
	}

	void SetItemHoverRow(int InHoverRow)
	{
		setProperty("itemHoverRow", InHoverRow);
		viewport()->setProperty("itemHoverRow", InHoverRow);
	}

	void UpdateHoverItemPaint(int InRow)
	{
		if (model() == nullptr || InRow < 0)
		{
			return;
		}

		const QModelIndex RowIndex = model()->index(InRow, 0);
		if (RowIndex.isValid())
		{
			update(visualRect(RowIndex));
		}
	}

	void ClearItemHoverState()
	{
		if (m_tooltip_show_timer_ != nullptr)
		{
			m_tooltip_show_timer_->stop();
		}
		HideItemTooltip();
		m_pending_hover_index_ = QModelIndex();
		const int PreviousHoverRow = m_hover_row_;
		m_hover_row_ = kAssetDragSourceNone;
		if (PreviousHoverRow != kAssetDragSourceNone)
		{
			SetItemHoverRow(kAssetDragSourceNone);
			UpdateHoverItemPaint(PreviousHoverRow);
		}
	}

	void UpdateItemHoverAt(const QPoint& InViewportPos)
	{
		const QModelIndex HitIndex = indexAt(InViewportPos);
		const int HoverRow = HitIndex.isValid() ? HitIndex.row() : kAssetDragSourceNone;
		if (HoverRow == m_hover_row_)
		{
			return;
		}

		const int PreviousHoverRow = m_hover_row_;
		m_hover_row_ = HoverRow;
		SetItemHoverRow(HoverRow);
		UpdateHoverItemPaint(PreviousHoverRow);
		UpdateHoverItemPaint(HoverRow);

		if (m_tooltip_show_timer_ != nullptr)
		{
			m_tooltip_show_timer_->stop();
		}
		HideItemTooltip();

		if (!HitIndex.isValid())
		{
			m_pending_hover_index_ = QModelIndex();
			return;
		}

		m_pending_hover_index_ = HitIndex;
		if (m_tooltip_show_timer_ != nullptr)
		{
			m_tooltip_show_timer_->start();
		}
	}

	void ShowPendingItemTooltip()
	{
		if (m_b_rubber_band_selecting_ || !m_pending_hover_index_.isValid()
			|| m_hover_row_ != m_pending_hover_index_.row())
		{
			return;
		}

		const QModelIndex Index = m_pending_hover_index_;
		const QString Name = Index.data(Qt::DisplayRole).toString();
		const QString TypeName = Index.data(static_cast<int>(EAssetListRole::TypeDisplayName)).toString();
		QString SoftPath;
		if (Index.data(static_cast<int>(EAssetListRole::IsFolder)).toBool())
		{
			SoftPath = Index.data(static_cast<int>(EAssetListRole::FolderPath)).toString();
		}
		else
		{
			SoftPath = Index.data(static_cast<int>(EAssetListRole::SoftObjectPath)).toString();
		}

		if (m_item_tooltip_ == nullptr)
		{
			m_item_tooltip_ = new AssetBrowserItemTooltipWidget();
		}

		m_item_tooltip_->SetContent(Name, TypeName, SoftPath);

		const QRect ItemRect = visualRect(Index);
		const QPoint TopLeftGlobal = viewport()->mapToGlobal(ItemRect.topLeft());
		const QRect ItemScreenRect(TopLeftGlobal, ItemRect.size());
		m_item_tooltip_->ShowNearGlobalPos(QCursor::pos(), ItemScreenRect);
	}

	QRubberBand* m_rubber_band_ = nullptr;
	QPoint m_rubber_band_origin_{};
	bool m_b_rubber_band_selecting_ = false;
	int m_hover_row_ = kAssetDragSourceNone;
	QModelIndex m_pending_hover_index_;
	QTimer* m_tooltip_show_timer_ = nullptr;
	AssetBrowserItemTooltipWidget* m_item_tooltip_ = nullptr;
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

void AssetBrowserPanelWidget::ApplyAssetGridTileSize(int InThumbnailSize)
{
	if (m_asset_grid_ == nullptr)
	{
		return;
	}

	const FAssetTileMetrics Metrics = BuildAssetTileMetrics(InThumbnailSize);
	m_asset_tile_thumbnail_size_ = Metrics.ThumbnailSize;
	m_asset_grid_->viewport()->setProperty("assetTileThumbnailSize", Metrics.ThumbnailSize);
	m_asset_grid_->setGridSize(QSize(Metrics.TileWidth, Metrics.TileHeight));
	m_asset_grid_->doItemsLayout();
	m_asset_grid_->viewport()->update();
}

void AssetBrowserPanelWidget::AdjustAssetGridTileSize(int InWheelDeltaY)
{
	if (InWheelDeltaY == 0)
	{
		return;
	}

	const int Step = InWheelDeltaY > 0 ? kAssetTileThumbnailStep : -kAssetTileThumbnailStep;
	const int NewThumbnailSize = m_asset_tile_thumbnail_size_ + Step;
	if (NewThumbnailSize == m_asset_tile_thumbnail_size_
		|| NewThumbnailSize < kMinAssetTileThumbnailSize
		|| NewThumbnailSize > kMaxAssetTileThumbnailSize)
	{
		return;
	}

	ApplyAssetGridTileSize(NewThumbnailSize);
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

	const int FolderWidth = qMax(160, TotalWidth * 17 / 100);
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

	m_type_filter_widget_ = new AssetBrowserTypeFilterWidget(this);
	ToolbarLayout->addWidget(m_type_filter_widget_);
	RootLayout->addLayout(ToolbarLayout);

	m_splitter_ = new QSplitter(Qt::Horizontal, this);
	m_folder_tree_ = new AssetFolderTreeWidget(m_splitter_);
	m_folder_tree_->setHeaderHidden(true);
	m_folder_tree_->setMinimumWidth(120);
	m_folder_tree_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_folder_tree_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_folder_tree_->setContextMenuPolicy(Qt::CustomContextMenu);
	AssetFolderTreeWidget* FolderTree = static_cast<AssetFolderTreeWidget*>(m_folder_tree_);
	FolderTree->ItemDropHandler =
		[this](
			const std::string& InTargetFolderPath,
			const std::vector<std::string>& InSoftObjectPaths,
			const std::vector<std::string>& InFolderPaths)
		{
			OnItemsDroppedToFolder(InTargetFolderPath, InSoftObjectPaths, InFolderPaths);
		};
	FolderTree->DeleteRequestedHandler =
		[this]()
		{
			DeleteSelectedFolders();
		};

	m_asset_grid_ = new AssetBrowserListView(m_splitter_);
	m_asset_grid_->setObjectName("AssetBrowserGrid");
	m_asset_grid_->setViewMode(QListView::IconMode);
	m_asset_grid_->setResizeMode(QListView::Fixed);
	m_asset_grid_->setMovement(QListView::Static);
	m_asset_grid_->setSpacing(8);
	m_asset_grid_->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_asset_grid_->setSelectionBehavior(QAbstractItemView::SelectItems);
	m_asset_grid_->setFocusPolicy(Qt::StrongFocus);
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
	static_cast<AssetBrowserListView*>(m_asset_grid_)->ItemDropHandler =
		[this](
			const std::string& InTargetFolderPath,
			const std::vector<std::string>& InSoftObjectPaths,
			const std::vector<std::string>& InFolderPaths)
		{
			OnItemsDroppedToFolder(InTargetFolderPath, InSoftObjectPaths, InFolderPaths);
		};
	static_cast<AssetBrowserListView*>(m_asset_grid_)->DeleteRequestedHandler =
		[this]()
		{
			DeleteSelectedGridItems();
		};
	static_cast<AssetBrowserListView*>(m_asset_grid_)->TileSizeWheelHandler =
		[this](int InWheelDeltaY)
		{
			AdjustAssetGridTileSize(InWheelDeltaY);
		};

	ApplyAssetGridTileSize(m_asset_tile_thumbnail_size_);

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

	m_delete_action_ = new QAction(tr("删除"), this);
	addAction(m_delete_action_);

	m_folder_tree_->installEventFilter(this);
	if (m_folder_tree_->viewport() != nullptr)
	{
		m_folder_tree_->viewport()->installEventFilter(this);
	}
	m_asset_grid_->installEventFilter(this);
	if (m_asset_grid_->viewport() != nullptr)
	{
		m_asset_grid_->viewport()->installEventFilter(this);
	}

	connect(m_search_edit_, &QLineEdit::textChanged, this, &AssetBrowserPanelWidget::OnSearchTextChanged);
	connect(
		m_type_filter_widget_,
		&AssetBrowserTypeFilterWidget::FilterChanged,
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
		m_asset_grid_->selectionModel(),
		&QItemSelectionModel::selectionChanged,
		this,
		&AssetBrowserPanelWidget::OnGridSelectionChanged);
	connect(
		m_asset_grid_->verticalScrollBar(),
		&QScrollBar::valueChanged,
		this,
		&AssetBrowserPanelWidget::OnGridScrolled);
	connect(m_rename_action_, &QAction::triggered, this, [this]()
	{
		if (IsFolderTreeFocused())
		{
			BeginRenameSelectedFolder();
			return;
		}

		if (IsAssetGridFocused())
		{
			BeginRenameSelectedGridItem();
		}
	});
	connect(m_copy_action_, &QAction::triggered, this, [this]()
	{
		if (m_asset_grid_->hasFocus() && BuildGridSelectionState().Capabilities.bCanCopy)
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
	connect(m_delete_action_, &QAction::triggered, this, [this]()
	{
		if (IsFolderTreeFocused())
		{
			DeleteSelectedFolders();
			return;
		}

		if (IsGridDeleteShortcutEnabled())
		{
			DeleteSelectedGridItems();
		}
	});

	installEventFilter(this);
}

bool AssetBrowserPanelWidget::IsAssetGridFocused() const
{
	if (m_asset_grid_ == nullptr)
	{
		return false;
	}

	QWidget* FocusWidget = QApplication::focusWidget();
	if (FocusWidget == nullptr)
	{
		return false;
	}

	return FocusWidget == m_asset_grid_
		|| FocusWidget == m_asset_grid_->viewport()
		|| m_asset_grid_->isAncestorOf(FocusWidget);
}

bool AssetBrowserPanelWidget::IsFolderTreeFocused() const
{
	if (m_folder_tree_ == nullptr)
	{
		return false;
	}

	QWidget* FocusWidget = QApplication::focusWidget();
	if (FocusWidget == nullptr)
	{
		return false;
	}

	return FocusWidget == m_folder_tree_
		|| FocusWidget == m_folder_tree_->viewport()
		|| m_folder_tree_->isAncestorOf(FocusWidget);
}

bool AssetBrowserPanelWidget::IsRenderViewportFocused() const
{
	QWidget* FocusWidget = QApplication::focusWidget();
	for (QWidget* Widget = FocusWidget; Widget != nullptr; Widget = Widget->parentWidget())
	{
		const QString ClassName = Widget->metaObject()->className();
		if (ClassName == QLatin1String("RenderViewportWidget")
			|| ClassName == QLatin1String("ViewportHostWidget"))
		{
			return true;
		}
	}

	return false;
}

bool AssetBrowserPanelWidget::IsGridDeleteShortcutEnabled() const
{
	if (!IsAssetGridFocused())
	{
		return false;
	}

	const FAssetBrowserGridSelectionState SelectionState = BuildGridSelectionState();
	return SelectionState.Capabilities.bCanDelete && !SelectionState.Items.empty();
}

bool AssetBrowserPanelWidget::IsAssetDeleteShortcutEnabled() const
{
	if (!IsAssetGridFocused() || GetSelectedAssetItems().empty())
	{
		return false;
	}

	QWidget* FocusWidget = QApplication::focusWidget();
	if (FocusWidget == m_search_edit_)
	{
		return false;
	}

	if (FocusWidget != nullptr)
	{
		if (qobject_cast<const QLineEdit*>(FocusWidget) != nullptr
			|| qobject_cast<const QAbstractSpinBox*>(FocusWidget) != nullptr)
		{
			return false;
		}
	}

	return true;
}

bool AssetBrowserPanelWidget::ContainsFolderTreeWidget(const QWidget* InWidget) const
{
	if (InWidget == nullptr || m_folder_tree_ == nullptr)
	{
		return false;
	}

	return InWidget == m_folder_tree_
		|| InWidget == m_folder_tree_->viewport()
		|| m_folder_tree_->isAncestorOf(InWidget);
}

bool AssetBrowserPanelWidget::ContainsItemGridWidget(const QWidget* InWidget) const
{
	if (InWidget == nullptr || m_asset_grid_ == nullptr)
	{
		return false;
	}

	return InWidget == m_asset_grid_
		|| InWidget == m_asset_grid_->viewport()
		|| m_asset_grid_->isAncestorOf(InWidget);
}

void AssetBrowserPanelWidget::ClearAssetGridSelection()
{
	if (m_asset_grid_ == nullptr || m_asset_grid_->selectionModel() == nullptr)
	{
		return;
	}

	if (!m_asset_grid_->selectionModel()->hasSelection())
	{
		return;
	}

	m_asset_grid_->clearSelection();
	m_asset_grid_->setCurrentIndex(QModelIndex());
	UpdateGridStatusLabel();
}

void AssetBrowserPanelWidget::ClearFolderTreeMultiSelection()
{
	if (m_folder_tree_ == nullptr)
	{
		return;
	}

	QTreeWidgetItem* CurrentItem = m_folder_tree_->currentItem();
	const QList<QTreeWidgetItem*> SelectedItems = m_folder_tree_->selectedItems();
	if (SelectedItems.size() <= 1 && (SelectedItems.isEmpty() || SelectedItems.front() == CurrentItem))
	{
		return;
	}

	m_folder_tree_->clearSelection();
	if (CurrentItem != nullptr)
	{
		CurrentItem->setSelected(true);
		m_folder_tree_->setCurrentItem(CurrentItem);
	}
}

void AssetBrowserPanelWidget::ClearAllSelections()
{
	ClearAssetGridSelection();
	ClearFolderTreeMultiSelection();
}

bool AssetBrowserPanelWidget::eventFilter(QObject* InWatched, QEvent* InEvent)
{
	const bool bIsAssetGridInputTarget =
		m_asset_grid_ != nullptr &&
		(InWatched == m_asset_grid_ || InWatched == m_asset_grid_->viewport());
	const bool bIsFolderTreeInputTarget =
		m_folder_tree_ != nullptr &&
		(InWatched == m_folder_tree_ || InWatched == m_folder_tree_->viewport());

	if (InEvent->type() == QEvent::KeyPress)
	{
		auto* KeyEvent = static_cast<QKeyEvent*>(InEvent);
		if (KeyEvent->key() == Qt::Key_F2)
		{
			if (bIsFolderTreeInputTarget)
			{
				BeginRenameSelectedFolder();
				return true;
			}
			if (bIsAssetGridInputTarget)
			{
				BeginRenameSelectedGridItem();
				return true;
			}
		}

		if (bIsFolderTreeInputTarget && KeyEvent->key() == Qt::Key_Delete)
		{
			DeleteSelectedFolders();
			return true;
		}

		if (bIsAssetGridInputTarget)
		{
			if (KeyEvent->matches(QKeySequence::Copy) && BuildGridSelectionState().Capabilities.bCanCopy)
			{
				CopySelectedAsset();
				return true;
			}
			if (KeyEvent->matches(QKeySequence::Paste) && m_paste_action_->isEnabled())
			{
				PasteCopiedAsset();
				return true;
			}
			if (KeyEvent->key() == Qt::Key_Delete && IsGridDeleteShortcutEnabled())
			{
				DeleteSelectedGridItems();
				return true;
			}
		}

		if (InWatched == this && KeyEvent->key() == Qt::Key_Delete)
		{
			if (IsFolderTreeFocused())
			{
				DeleteSelectedFolders();
				return true;
			}

			if (IsGridDeleteShortcutEnabled())
			{
				DeleteSelectedGridItems();
				return true;
			}
		}
	}

	return QWidget::eventFilter(InWatched, InEvent);
}

bool AssetBrowserPanelWidget::CanRenameFolderPath(const std::string& InFolderPath) const
{
	return CanRenameFolderPathForInteraction(InFolderPath);
}

bool AssetBrowserPanelWidget::CanRenameFolderItem(const QTreeWidgetItem* InItem) const
{
	if (InItem == nullptr)
	{
		return false;
	}

	const std::string FolderPath = InItem->data(0, Qt::UserRole).toString().toUtf8().toStdString();
	return CanRenameFolderPath(FolderPath);
}

bool AssetBrowserPanelWidget::CanDeleteFolderItem(const QTreeWidgetItem* InItem) const
{
	return CanRenameFolderItem(InItem);
}

std::vector<std::string> AssetBrowserPanelWidget::GetSelectedDeletableFolderPaths() const
{
	std::vector<std::string> FolderPaths;
	if (m_folder_tree_ == nullptr)
	{
		return FolderPaths;
	}

	const QList<QTreeWidgetItem*> SelectedItems = m_folder_tree_->selectedItems();
	for (QTreeWidgetItem* Item : SelectedItems)
	{
		if (!CanDeleteFolderItem(Item))
		{
			continue;
		}

		const std::string FolderPath = Item->data(0, Qt::UserRole).toString().toStdString();
		if (!FolderPath.empty())
		{
			FolderPaths.push_back(FolderPath);
		}
	}

	return DeduplicateNestedFolderPaths(std::move(FolderPaths));
}

std::vector<std::string> AssetBrowserPanelWidget::GetSelectedGridFolderPaths() const
{
	std::vector<std::string> FolderPaths;
	if (m_asset_grid_ == nullptr || m_list_model_ == nullptr || m_asset_grid_->selectionModel() == nullptr)
	{
		return FolderPaths;
	}

	const QModelIndexList SelectedIndexes = m_asset_grid_->selectionModel()->selectedIndexes();
	for (const QModelIndex& Index : SelectedIndexes)
	{
		if (!Index.isValid() || !Index.data(static_cast<int>(EAssetListRole::IsFolder)).toBool())
		{
			continue;
		}

		const std::string FolderPath =
			Index.data(static_cast<int>(EAssetListRole::FolderPath)).toString().toUtf8().toStdString();
		if (!CanRenameFolderPath(FolderPath))
		{
			continue;
		}

		FolderPaths.push_back(FolderPath);
	}

	return DeduplicateNestedFolderPaths(std::move(FolderPaths));
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
	if (m_folder_tree_->viewport() != nullptr)
	{
		m_folder_tree_->viewport()->setFocus(Qt::OtherFocusReason);
	}
	m_folder_tree_->editItem(CurrentItem, 0);
}

bool AssetBrowserPanelWidget::RenameFolderByPath(const std::string& InFolderPath, const std::string& InNewFolderName)
{
	if (!CanRenameFolderPath(InFolderPath))
	{
		return false;
	}

	const size_t LastSlash = InFolderPath.find_last_of('/');
	const std::string OldFolderName =
		(LastSlash == std::string::npos) ? InFolderPath : InFolderPath.substr(LastSlash + 1);
	if (InNewFolderName == OldFolderName)
	{
		return true;
	}

	std::string ErrorMessage;
	ClearContentDiskWatchPaths();
	const bool bRenameSucceeded = FAssetRenameService::RenameFolder(InFolderPath, InNewFolderName, &ErrorMessage);
	UpdateContentDiskWatchPaths();
	if (!bRenameSucceeded)
	{
		QMessageBox::warning(
			this,
			tr("重命名失败"),
			tr("无法重命名文件夹: %1").arg(QString::fromUtf8(ErrorMessage.c_str())));
		m_last_registry_revision_ = 0;
		RebuildFolderTree();
		return false;
	}

	const std::string ParentPath = (LastSlash == std::string::npos) ? std::string() : InFolderPath.substr(0, LastSlash);
	const std::string NewFolderPath =
		ParentPath.empty() ? InNewFolderName : ParentPath + "/" + InNewFolderName;
	if (m_selected_folder_path_ == InFolderPath || IsSubfolderPath(m_selected_folder_path_, InFolderPath))
	{
		m_selected_folder_path_ = NewFolderPath;
		for (std::string& SelectedFolderPath : m_selected_folder_paths_)
		{
			if (SelectedFolderPath == InFolderPath)
			{
				SelectedFolderPath = NewFolderPath;
			}
			else if (IsSubfolderPath(SelectedFolderPath, InFolderPath))
			{
				SelectedFolderPath = NewFolderPath + SelectedFolderPath.substr(InFolderPath.size());
			}
		}
		NormalizeSelectedFolderPaths(m_selected_folder_paths_);
	}

	m_last_registry_revision_ = 0;
	RefreshFromRegistry();
	return true;
}

void AssetBrowserPanelWidget::BeginRenameSelectedGridFolder()
{
	std::string TargetFolderPath;
	const QModelIndex CurrentIndex = m_asset_grid_->currentIndex();
	if (CurrentIndex.isValid() && CurrentIndex.data(static_cast<int>(EAssetListRole::IsFolder)).toBool())
	{
		TargetFolderPath =
			CurrentIndex.data(static_cast<int>(EAssetListRole::FolderPath)).toString().toUtf8().toStdString();
	}
	else
	{
		const std::vector<std::string> SelectedFolderPaths = GetSelectedGridFolderPaths();
		if (SelectedFolderPaths.size() != 1)
		{
			return;
		}

		TargetFolderPath = SelectedFolderPaths.front();
	}

	if (!CanRenameFolderPath(TargetFolderPath))
	{
		return;
	}

	const QString CurrentName = FolderPathToDisplayName(TargetFolderPath);
	const QString NewName = QInputDialog::getText(
		this,
		tr("重命名文件夹"),
		tr("新文件夹名称:"),
		QLineEdit::Normal,
		CurrentName);
	if (NewName.trimmed().isEmpty() || NewName.trimmed() == CurrentName.trimmed())
	{
		return;
	}

	RenameFolderByPath(TargetFolderPath, NewName.trimmed().toUtf8().toStdString());
}

void AssetBrowserPanelWidget::BeginRenameSelectedGridItem()
{
	const FAssetBrowserGridSelectionState SelectionState = BuildGridSelectionState();
	if (!SelectionState.Capabilities.bCanRename || SelectionState.Items.size() != 1)
	{
		return;
	}

	const FAssetBrowserSelectedGridItem& SelectedItem = SelectionState.Items.front();
	if (SelectedItem.Kind == EAssetBrowserItemKind::Folder)
	{
		BeginRenameSelectedGridFolder();
		return;
	}

	BeginRenameSelectedAsset();
}

void AssetBrowserPanelWidget::OnFolderItemChanged(QTreeWidgetItem* InItem, int InColumn)
{
	(void)InColumn;
	if (m_is_folder_rename_in_progress_ || InItem == nullptr || !CanRenameFolderItem(InItem))
	{
		return;
	}

	const std::string FolderPath = InItem->data(0, Qt::UserRole).toString().toUtf8().toStdString();
	const std::string NewFolderName = InItem->text(0).trimmed().toUtf8().toStdString();
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

	RenameFolderByPath(FolderPath, NewFolderName);
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
		m_folder_tree_->clearSelection();
		FoundItem->setSelected(true);
		m_folder_tree_->setCurrentItem(FoundItem);
		m_is_folder_rename_in_progress_ = false;
		SyncSelectedFolderPathsFromTree();
		return;
	}

	if (m_folder_tree_->topLevelItemCount() > 0)
	{
		m_folder_tree_->setCurrentItem(m_folder_tree_->topLevelItem(0));
		SyncSelectedFolderPathsFromTree();
	}
}

void AssetBrowserPanelWidget::SyncSelectedFolderPathsFromTree()
{
	m_selected_folder_paths_.clear();
	if (m_folder_tree_ == nullptr)
	{
		m_selected_folder_paths_.push_back("All");
		m_selected_folder_path_ = "All";
		return;
	}

	const QList<QTreeWidgetItem*> SelectedItems = m_folder_tree_->selectedItems();
	for (QTreeWidgetItem* Item : SelectedItems)
	{
		if (Item == nullptr)
		{
			continue;
		}

		const std::string FolderPath = Item->data(0, Qt::UserRole).toString().toStdString();
		if (!FolderPath.empty())
		{
			m_selected_folder_paths_.push_back(FolderPath);
		}
	}

	if (m_selected_folder_paths_.empty())
	{
		if (QTreeWidgetItem* CurrentItem = m_folder_tree_->currentItem())
		{
			const std::string FolderPath = CurrentItem->data(0, Qt::UserRole).toString().toStdString();
			if (!FolderPath.empty())
			{
				m_selected_folder_paths_.push_back(FolderPath);
			}
		}
	}

	NormalizeSelectedFolderPaths(m_selected_folder_paths_);

	if (QTreeWidgetItem* CurrentItem = m_folder_tree_->currentItem())
	{
		m_selected_folder_path_ = CurrentItem->data(0, Qt::UserRole).toString().toStdString();
	}
	else if (!m_selected_folder_paths_.empty())
	{
		m_selected_folder_path_ = m_selected_folder_paths_.front();
	}
	else
	{
		m_selected_folder_path_ = "All";
		m_selected_folder_paths_.push_back("All");
	}
}

void AssetBrowserPanelWidget::RestoreFolderTreeSelection()
{
	if (m_folder_tree_ == nullptr)
	{
		return;
	}

	m_folder_tree_->clearSelection();
	for (const std::string& FolderPath : m_selected_folder_paths_)
	{
		if (QTreeWidgetItem* FoundItem = FindFolderTreeItemByPath(FolderPath))
		{
			for (QTreeWidgetItem* ParentItem = FoundItem->parent(); ParentItem != nullptr; ParentItem = ParentItem->parent())
			{
				ParentItem->setExpanded(true);
			}
			FoundItem->setSelected(true);
		}
	}

	if (QTreeWidgetItem* CurrentItem = FindFolderTreeItemByPath(m_selected_folder_path_))
	{
		m_is_folder_rename_in_progress_ = true;
		m_folder_tree_->setCurrentItem(CurrentItem);
		m_is_folder_rename_in_progress_ = false;
	}
	else if (m_folder_tree_->topLevelItemCount() > 0)
	{
		m_folder_tree_->setCurrentItem(m_folder_tree_->topLevelItem(0));
	}
}

const std::vector<std::string>& AssetBrowserPanelWidget::GetActiveFolderFilterPaths() const
{
	return m_selected_folder_paths_;
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
	const std::vector<const FAssetBrowserListItem*> SelectedItems = GetSelectedAssetItems();
	if (SelectedItems.empty())
	{
		return;
	}

	std::vector<FAssetRegistryEntry> CopiedEntries;
	CopiedEntries.reserve(SelectedItems.size());
	for (const FAssetBrowserListItem* Item : SelectedItems)
	{
		if (Item == nullptr)
		{
			continue;
		}

		CopiedEntries.push_back(Item->Entry);
	}

	if (CopiedEntries.empty())
	{
		return;
	}

	m_copied_asset_entries_ = std::move(CopiedEntries);
	if (m_paste_action_ != nullptr)
	{
		m_paste_action_->setEnabled(true);
	}
}

void AssetBrowserPanelWidget::PasteCopiedAsset()
{
	if (m_copied_asset_entries_.empty())
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

	QStringList FailedMessages;
	int SuccessCount = 0;
	for (const FAssetRegistryEntry& Entry : m_copied_asset_entries_)
	{
		std::string NewSoftObjectPath;
		std::string ErrorMessage;
		if (!FAssetDuplicateService::DuplicateAsset(
				Entry,
				m_selected_folder_path_,
				&NewSoftObjectPath,
				&ErrorMessage))
		{
			FailedMessages.push_back(
				tr("%1: %2")
					.arg(QString::fromStdString(Entry.ObjectName))
					.arg(QString::fromStdString(ErrorMessage)));
			continue;
		}

		++SuccessCount;
	}

	if (SuccessCount > 0)
	{
		RefreshAfterAssetDiskMutation();
	}

	if (!FailedMessages.isEmpty())
	{
		QMessageBox::warning(
			this,
			tr("粘贴失败"),
			tr("部分资产无法粘贴:\n%1").arg(FailedMessages.join('\n')));
	}
}

std::vector<const FAssetBrowserListItem*> AssetBrowserPanelWidget::GetSelectedAssetItems() const
{
	std::vector<const FAssetBrowserListItem*> SelectedItems;
	if (m_asset_grid_ == nullptr || m_list_model_ == nullptr || m_asset_grid_->selectionModel() == nullptr)
	{
		return SelectedItems;
	}

	const QModelIndexList SelectedIndexes = m_asset_grid_->selectionModel()->selectedIndexes();
	for (const QModelIndex& Index : SelectedIndexes)
	{
		if (!Index.isValid() || Index.data(static_cast<int>(EAssetListRole::IsFolder)).toBool())
		{
			continue;
		}

		const FAssetBrowserListItem* Item = m_list_model_->GetItemAt(Index.row());
		if (Item != nullptr)
		{
			SelectedItems.push_back(Item);
		}
	}

	return SelectedItems;
}

std::vector<FAssetBrowserSelectedGridItem> AssetBrowserPanelWidget::GetSelectedGridItems() const
{
	std::vector<FAssetBrowserSelectedGridItem> SelectedItems;
	if (m_asset_grid_ == nullptr || m_list_model_ == nullptr || m_asset_grid_->selectionModel() == nullptr)
	{
		return SelectedItems;
	}

	const QModelIndexList SelectedIndexes = m_asset_grid_->selectionModel()->selectedIndexes();
	for (const QModelIndex& Index : SelectedIndexes)
	{
		if (!Index.isValid())
		{
			continue;
		}

		FAssetBrowserSelectedGridItem SelectedItem;
		if (Index.data(static_cast<int>(EAssetListRole::IsFolder)).toBool())
		{
			SelectedItem.Kind = EAssetBrowserItemKind::Folder;
			SelectedItem.FolderPath =
				Index.data(static_cast<int>(EAssetListRole::FolderPath)).toString().toUtf8().toStdString();
			if (!CanRenameFolderPath(SelectedItem.FolderPath))
			{
				continue;
			}
		}
		else
		{
			SelectedItem.Kind = EAssetBrowserItemKind::Asset;
			SelectedItem.AssetListItem = m_list_model_->GetItemAt(Index.row());
			if (SelectedItem.AssetListItem == nullptr)
			{
				continue;
			}
		}

		SelectedItems.push_back(std::move(SelectedItem));
	}

	return SelectedItems;
}

FAssetBrowserGridSelectionState AssetBrowserPanelWidget::BuildGridSelectionState() const
{
	return ::BuildGridSelectionState(GetSelectedGridItems());
}

void AssetBrowserPanelWidget::DuplicateSelectedAssets()
{
	const std::vector<const FAssetBrowserListItem*> SelectedItems = GetSelectedAssetItems();
	if (SelectedItems.empty())
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

	QStringList FailedMessages;
	int SuccessCount = 0;
	for (const FAssetBrowserListItem* Item : SelectedItems)
	{
		if (Item == nullptr)
		{
			continue;
		}

		std::string NewSoftObjectPath;
		std::string ErrorMessage;
		if (!FAssetDuplicateService::DuplicateAsset(
				Item->Entry,
				m_selected_folder_path_,
				&NewSoftObjectPath,
				&ErrorMessage))
		{
			FailedMessages.push_back(
				tr("%1: %2")
					.arg(QString::fromStdString(Item->Entry.ObjectName))
					.arg(QString::fromStdString(ErrorMessage)));
			continue;
		}

		++SuccessCount;
	}

	if (SuccessCount > 0)
	{
		RefreshAfterAssetDiskMutation();
	}

	if (!FailedMessages.isEmpty())
	{
		QMessageBox::warning(
			this,
			tr("创建副本失败"),
			tr("部分资产无法创建副本:\n%1").arg(FailedMessages.join('\n')));
	}
}

void AssetBrowserPanelWidget::ReimportSelectedAssets()
{
	const std::vector<const FAssetBrowserListItem*> SelectedItems = GetSelectedAssetItems();
	if (SelectedItems.empty())
	{
		return;
	}

	QStringList FailedMessages;
	int SuccessCount = 0;
	for (const FAssetBrowserListItem* Item : SelectedItems)
	{
		if (Item == nullptr || !AssetTypeInfo::IsMeshAssetType(Item->Entry.Type))
		{
			continue;
		}

		if (Item->Entry.SourceFile.empty())
		{
			FailedMessages.push_back(
				tr("%1: 缺少 SourceFile").arg(QString::fromStdString(Item->Entry.ObjectName)));
			continue;
		}

		std::string ErrorMessage;
		const std::string SoftPath = FSoftObjectPath::Build(Item->Entry.AssetPath, Item->Entry.ObjectName);
		if (!UMeshImportFactory::Reimport(SoftPath, Item->Entry.SourceFile, &ErrorMessage))
		{
			FailedMessages.push_back(
				tr("%1: %2")
					.arg(QString::fromStdString(Item->Entry.ObjectName))
					.arg(QString::fromStdString(ErrorMessage)));
			continue;
		}

		FAssetThumbnailService::Get().InvalidateEntry(Item->Entry);
		++SuccessCount;
	}

	if (SuccessCount > 0)
	{
		RefreshFromRegistry();
	}

	if (!FailedMessages.isEmpty())
	{
		QMessageBox::warning(
			this,
			tr("Reimport 失败"),
			tr("部分资产无法 Reimport:\n%1").arg(FailedMessages.join('\n')));
	}
	else if (SuccessCount == 0)
	{
		QMessageBox::warning(this, tr("Reimport"), tr("所选资产中没有可 Reimport 的 Mesh 资产。"));
	}
}

bool AssetBrowserPanelWidget::TryHandleDeleteShortcut()
{
	if (IsFolderTreeFocused())
	{
		const std::vector<std::string> FolderPaths = GetSelectedDeletableFolderPaths();
		if (!FolderPaths.empty())
		{
			DeleteSelectedFolders();
			return true;
		}
	}

	if (IsGridDeleteShortcutEnabled())
	{
		DeleteSelectedGridItems();
		return true;
	}

	return false;
}

void AssetBrowserPanelWidget::DeleteSelectedFolders()
{
	const std::vector<std::string> FolderPaths = GetSelectedDeletableFolderPaths();
	if (FolderPaths.empty())
	{
		return;
	}

	size_t TotalAssetCount = 0;
	QStringList FolderNames;
	for (const std::string& FolderPath : FolderPaths)
	{
		TotalAssetCount += FAssetDeleteService::CountAssetsInFolder(m_all_entries_, FolderPath);
		FolderNames.push_back(FolderPathToDisplayName(FolderPath));
	}

	QString ConfirmMessage;
	if (FolderPaths.size() == 1)
	{
		ConfirmMessage = tr("确定要删除文件夹 \"%1\" 吗？\n该文件夹下共有 %2 个资产文件。\n此操作不可撤销。")
			.arg(FolderNames.front())
			.arg(static_cast<qulonglong>(TotalAssetCount));
	}
	else
	{
		ConfirmMessage = tr("确定要删除选中的 %1 个文件夹吗？\n共包含 %2 个资产文件。\n此操作不可撤销。\n\n%3")
			.arg(FolderPaths.size())
			.arg(static_cast<qulonglong>(TotalAssetCount))
			.arg(FolderNames.join('\n'));
	}

	const QMessageBox::StandardButton ConfirmResult = QMessageBox::question(
		this,
		tr("删除文件夹"),
		ConfirmMessage,
		QMessageBox::Yes | QMessageBox::No,
		QMessageBox::No);
	if (ConfirmResult != QMessageBox::Yes)
	{
		return;
	}

	ClearContentDiskWatchPaths();

	QStringList FailedMessages;
	std::vector<std::string> DeletedSoftPaths;
	int SuccessCount = 0;
	for (const std::string& FolderPath : FolderPaths)
	{
		std::vector<std::string> FolderDeletedSoftPaths;
		std::string ErrorMessage;
		if (!FAssetDeleteService::DeleteFolder(m_all_entries_, FolderPath, &FolderDeletedSoftPaths, &ErrorMessage))
		{
			FailedMessages.push_back(
				tr("%1: %2")
					.arg(FolderPathToDisplayName(FolderPath))
					.arg(QString::fromStdString(ErrorMessage)));
			continue;
		}

		DeletedSoftPaths.insert(
			DeletedSoftPaths.end(),
			FolderDeletedSoftPaths.begin(),
			FolderDeletedSoftPaths.end());
		++SuccessCount;
	}

	UpdateContentDiskWatchPaths();

	if (SuccessCount > 0)
	{
		if (!m_copied_asset_entries_.empty())
		{
			m_copied_asset_entries_.erase(
				std::remove_if(
					m_copied_asset_entries_.begin(),
					m_copied_asset_entries_.end(),
					[&DeletedSoftPaths](const FAssetRegistryEntry& InEntry)
					{
						const std::string CopiedSoftPath =
							FSoftObjectPath::Build(InEntry.AssetPath, InEntry.ObjectName);
						return std::find(
							DeletedSoftPaths.begin(),
							DeletedSoftPaths.end(),
							CopiedSoftPath) != DeletedSoftPaths.end();
					}),
				m_copied_asset_entries_.end());
			if (m_paste_action_ != nullptr)
			{
				m_paste_action_->setEnabled(!m_copied_asset_entries_.empty());
			}
		}

		if (m_game_app_ != nullptr && m_game_app_->HasActiveLevel())
		{
			ULevel* ActiveLevel = m_game_app_->GetActiveLevel();
			if (ActiveLevel != nullptr)
			{
				for (const std::string& SoftPath : DeletedSoftPaths)
				{
					ActiveLevel->UnloadMeshAssetReferences(SoftPath);
				}
			}
			m_game_app_->RefreshActiveLevelRender();
		}

		const bool bCurrentFolderDeleted = std::find(
			FolderPaths.begin(),
			FolderPaths.end(),
			m_selected_folder_path_) != FolderPaths.end()
			|| std::any_of(
				FolderPaths.begin(),
				FolderPaths.end(),
				[this](const std::string& InDeletedFolderPath)
				{
					return IsSubfolderPath(m_selected_folder_path_, InDeletedFolderPath);
				});
		if (bCurrentFolderDeleted)
		{
			m_selected_folder_path_ = "All";
			m_selected_folder_paths_ = {"All"};
		}

		RefreshAfterAssetDiskMutation();
	}

	if (!FailedMessages.isEmpty())
	{
		QMessageBox::warning(
			this,
			tr("删除失败"),
			tr("部分文件夹无法删除:\n%1").arg(FailedMessages.join('\n')));
	}
}

void AssetBrowserPanelWidget::DeleteSelectedAssets()
{
	const std::vector<const FAssetBrowserListItem*> SelectedItems = GetSelectedAssetItems();
	if (SelectedItems.empty())
	{
		return;
	}

	QStringList AssetNames;
	for (const FAssetBrowserListItem* Item : SelectedItems)
	{
		if (Item != nullptr)
		{
			AssetNames.push_back(QString::fromStdString(Item->Entry.ObjectName));
		}
	}

	QString ConfirmMessage;
	if (SelectedItems.size() == 1)
	{
		ConfirmMessage = tr("确定要删除资产 \"%1\" 吗？此操作不可撤销。").arg(AssetNames.front());
	}
	else
	{
		ConfirmMessage = tr("确定要删除选中的 %1 个资产吗？此操作不可撤销。\n\n%2")
			.arg(SelectedItems.size())
			.arg(AssetNames.join('\n'));
	}

	const QMessageBox::StandardButton ConfirmResult = QMessageBox::question(
		this,
		tr("删除资产"),
		ConfirmMessage,
		QMessageBox::Yes | QMessageBox::No,
		QMessageBox::No);
	if (ConfirmResult != QMessageBox::Yes)
	{
		return;
	}

	QStringList FailedMessages;
	std::vector<std::string> DeletedSoftPaths;
	int SuccessCount = 0;
	for (const FAssetBrowserListItem* Item : SelectedItems)
	{
		if (Item == nullptr)
		{
			continue;
		}

		const std::string SoftPath = FSoftObjectPath::Build(Item->Entry.AssetPath, Item->Entry.ObjectName);
		std::string ErrorMessage;
		if (!FAssetDeleteService::DeleteAsset(Item->Entry, &ErrorMessage))
		{
			FailedMessages.push_back(
				tr("%1: %2")
					.arg(QString::fromStdString(Item->Entry.ObjectName))
					.arg(QString::fromStdString(ErrorMessage)));
			continue;
		}

		DeletedSoftPaths.push_back(SoftPath);
		++SuccessCount;
	}

	if (SuccessCount > 0)
	{
		if (!m_copied_asset_entries_.empty())
		{
			m_copied_asset_entries_.erase(
				std::remove_if(
					m_copied_asset_entries_.begin(),
					m_copied_asset_entries_.end(),
					[&DeletedSoftPaths](const FAssetRegistryEntry& InEntry)
					{
						const std::string CopiedSoftPath =
							FSoftObjectPath::Build(InEntry.AssetPath, InEntry.ObjectName);
						return std::find(
							DeletedSoftPaths.begin(),
							DeletedSoftPaths.end(),
							CopiedSoftPath) != DeletedSoftPaths.end();
					}),
				m_copied_asset_entries_.end());
			if (m_paste_action_ != nullptr)
			{
				m_paste_action_->setEnabled(!m_copied_asset_entries_.empty());
			}
		}

		if (m_game_app_ != nullptr && m_game_app_->HasActiveLevel())
		{
			ULevel* ActiveLevel = m_game_app_->GetActiveLevel();
			if (ActiveLevel != nullptr)
			{
				for (const std::string& SoftPath : DeletedSoftPaths)
				{
					ActiveLevel->UnloadMeshAssetReferences(SoftPath);
				}
			}
			m_game_app_->RefreshActiveLevelRender();
		}

		RefreshAfterAssetDiskMutation();
	}

	if (!FailedMessages.isEmpty())
	{
		QMessageBox::warning(
			this,
			tr("删除失败"),
			tr("部分资产无法删除:\n%1").arg(FailedMessages.join('\n')));
	}
}

void AssetBrowserPanelWidget::DeleteSelectedGridItems()
{
	const FAssetBrowserGridSelectionState SelectionState = BuildGridSelectionState();
	if (!SelectionState.Capabilities.bCanDelete || SelectionState.Items.empty())
	{
		return;
	}

	std::vector<std::string> FolderPaths;
	std::vector<const FAssetBrowserListItem*> AssetItems;
	QStringList ItemDisplayNames;
	for (const FAssetBrowserSelectedGridItem& Item : SelectionState.Items)
	{
		if (Item.Kind == EAssetBrowserItemKind::Folder)
		{
			FolderPaths.push_back(Item.FolderPath);
			ItemDisplayNames.push_back(FolderPathToDisplayName(Item.FolderPath));
		}
		else if (Item.AssetListItem != nullptr)
		{
			AssetItems.push_back(Item.AssetListItem);
			ItemDisplayNames.push_back(QString::fromStdString(Item.AssetListItem->Entry.ObjectName));
		}
	}

	FolderPaths = DeduplicateNestedFolderPaths(std::move(FolderPaths));

	size_t NestedAssetCount = 0;
	for (const std::string& FolderPath : FolderPaths)
	{
		NestedAssetCount += FAssetDeleteService::CountAssetsInFolder(m_all_entries_, FolderPath);
	}

	QString ConfirmMessage;
	const int TotalItemCount = static_cast<int>(FolderPaths.size() + AssetItems.size());
	if (SelectionState.bIsMixedTypeSelection)
	{
		ConfirmMessage = tr("确定要删除选中的 %1 个项目吗？\n• %2 个文件夹（共包含 %3 个资产文件）\n• %4 个资产\n此操作不可撤销。\n\n%5")
			.arg(TotalItemCount)
			.arg(FolderPaths.size())
			.arg(static_cast<qulonglong>(NestedAssetCount))
			.arg(AssetItems.size())
			.arg(ItemDisplayNames.join('\n'));
	}
	else if (FolderPaths.size() == 1 && AssetItems.empty())
	{
		ConfirmMessage = tr("确定要删除文件夹 \"%1\" 吗？\n该文件夹下共有 %2 个资产文件。\n此操作不可撤销。")
			.arg(ItemDisplayNames.front())
			.arg(static_cast<qulonglong>(NestedAssetCount));
	}
	else if (AssetItems.size() == 1 && FolderPaths.empty())
	{
		ConfirmMessage = tr("确定要删除资产 \"%1\" 吗？此操作不可撤销。").arg(ItemDisplayNames.front());
	}
	else if (!FolderPaths.empty())
	{
		ConfirmMessage = tr("确定要删除选中的 %1 个文件夹吗？\n共包含 %2 个资产文件。\n此操作不可撤销。\n\n%3")
			.arg(FolderPaths.size())
			.arg(static_cast<qulonglong>(NestedAssetCount))
			.arg(ItemDisplayNames.join('\n'));
	}
	else
	{
		ConfirmMessage = tr("确定要删除选中的 %1 个资产吗？此操作不可撤销。\n\n%2")
			.arg(AssetItems.size())
			.arg(ItemDisplayNames.join('\n'));
	}

	const QMessageBox::StandardButton ConfirmResult = QMessageBox::question(
		this,
		tr("删除项目"),
		ConfirmMessage,
		QMessageBox::Yes | QMessageBox::No,
		QMessageBox::No);
	if (ConfirmResult != QMessageBox::Yes)
	{
		return;
	}

	ClearContentDiskWatchPaths();

	QStringList FailedMessages;
	std::vector<std::string> DeletedSoftPaths;
	int SuccessCount = 0;

	for (const std::string& FolderPath : FolderPaths)
	{
		std::vector<std::string> FolderDeletedSoftPaths;
		std::string ErrorMessage;
		if (!FAssetDeleteService::DeleteFolder(m_all_entries_, FolderPath, &FolderDeletedSoftPaths, &ErrorMessage))
		{
			FailedMessages.push_back(
				tr("%1: %2")
					.arg(FolderPathToDisplayName(FolderPath))
					.arg(QString::fromStdString(ErrorMessage)));
			continue;
		}

		DeletedSoftPaths.insert(
			DeletedSoftPaths.end(),
			FolderDeletedSoftPaths.begin(),
			FolderDeletedSoftPaths.end());
		++SuccessCount;
	}

	for (const FAssetBrowserListItem* Item : AssetItems)
	{
		if (Item == nullptr)
		{
			continue;
		}

		const std::string SoftPath = FSoftObjectPath::Build(Item->Entry.AssetPath, Item->Entry.ObjectName);
		std::string ErrorMessage;
		if (!FAssetDeleteService::DeleteAsset(Item->Entry, &ErrorMessage))
		{
			FailedMessages.push_back(
				tr("%1: %2")
					.arg(QString::fromStdString(Item->Entry.ObjectName))
					.arg(QString::fromStdString(ErrorMessage)));
			continue;
		}

		DeletedSoftPaths.push_back(SoftPath);
		++SuccessCount;
	}

	UpdateContentDiskWatchPaths();

	if (SuccessCount > 0)
	{
		if (!m_copied_asset_entries_.empty())
		{
			m_copied_asset_entries_.erase(
				std::remove_if(
					m_copied_asset_entries_.begin(),
					m_copied_asset_entries_.end(),
					[&DeletedSoftPaths](const FAssetRegistryEntry& InEntry)
					{
						const std::string CopiedSoftPath =
							FSoftObjectPath::Build(InEntry.AssetPath, InEntry.ObjectName);
						return std::find(
							DeletedSoftPaths.begin(),
							DeletedSoftPaths.end(),
							CopiedSoftPath) != DeletedSoftPaths.end();
					}),
				m_copied_asset_entries_.end());
			if (m_paste_action_ != nullptr)
			{
				m_paste_action_->setEnabled(!m_copied_asset_entries_.empty());
			}
		}

		if (m_game_app_ != nullptr && m_game_app_->HasActiveLevel())
		{
			ULevel* ActiveLevel = m_game_app_->GetActiveLevel();
			if (ActiveLevel != nullptr)
			{
				for (const std::string& SoftPath : DeletedSoftPaths)
				{
					ActiveLevel->UnloadMeshAssetReferences(SoftPath);
				}
			}
			m_game_app_->RefreshActiveLevelRender();
		}

		const bool bCurrentFolderDeleted = std::any_of(
			FolderPaths.begin(),
			FolderPaths.end(),
			[this](const std::string& InDeletedFolderPath)
			{
				return m_selected_folder_path_ == InDeletedFolderPath
					|| IsSubfolderPath(m_selected_folder_path_, InDeletedFolderPath);
			});
		if (bCurrentFolderDeleted)
		{
			m_selected_folder_path_ = "All";
			m_selected_folder_paths_ = {"All"};
		}

		RefreshAfterAssetDiskMutation();
	}

	if (!FailedMessages.isEmpty())
	{
		QMessageBox::warning(
			this,
			tr("删除失败"),
			tr("部分项目无法删除:\n%1").arg(FailedMessages.join('\n')));
	}
}

void AssetBrowserPanelWidget::OnItemsDroppedToFolder(
	const std::string& InTargetFolderPath,
	const std::vector<std::string>& InSoftObjectPaths,
	const std::vector<std::string>& InFolderPaths)
{
	if (InSoftObjectPaths.empty() && InFolderPaths.empty())
	{
		return;
	}

	const std::vector<std::string> RootFolderPaths = DeduplicateNestedFolderPaths(InFolderPaths);
	if (!RootFolderPaths.empty())
	{
		ClearContentDiskWatchPaths();
	}

	QStringList FailedMessages;
	int SuccessCount = 0;

	for (const std::string& SoftObjectPath : InSoftObjectPaths)
	{
		const std::optional<FAssetRegistryEntry> Entry = FAssetRegistry::Get().FindBySoftPath(SoftObjectPath);
		if (!Entry.has_value())
		{
			FailedMessages.push_back(tr("未找到资产: %1").arg(QString::fromStdString(SoftObjectPath)));
			continue;
		}

		std::string ErrorMessage;
		if (!FAssetRenameService::MoveAssetToFolder(Entry.value(), InTargetFolderPath, nullptr, &ErrorMessage))
		{
			FailedMessages.push_back(
				tr("%1: %2")
					.arg(QString::fromStdString(Entry->ObjectName))
					.arg(QString::fromStdString(ErrorMessage)));
			continue;
		}

		++SuccessCount;
	}

	for (const std::string& FolderPath : RootFolderPaths)
	{
		if (!CanMoveFolderInto(FolderPath, InTargetFolderPath))
		{
			FailedMessages.push_back(
				tr("%1: %2")
					.arg(FolderPathToDisplayName(FolderPath))
					.arg(tr("无法移动到该文件夹。")));
			continue;
		}

		std::string ErrorMessage;
		if (!FAssetRenameService::MoveFolder(FolderPath, InTargetFolderPath, &ErrorMessage))
		{
			FailedMessages.push_back(
				tr("%1: %2")
					.arg(FolderPathToDisplayName(FolderPath))
					.arg(QString::fromStdString(ErrorMessage)));
			continue;
		}

		++SuccessCount;
	}

	if (SuccessCount > 0)
	{
		m_selected_folder_path_ = InTargetFolderPath;
		RefreshAfterAssetDiskMutation();
		SelectFolderTreeItemByPath(InTargetFolderPath);
	}
	else if (!RootFolderPaths.empty())
	{
		UpdateContentDiskWatchPaths();
	}

	if (!FailedMessages.isEmpty())
	{
		QMessageBox::warning(
			this,
			tr("移动失败"),
			tr("部分项目无法移动:\n%1").arg(FailedMessages.join('\n')));
	}
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

void AssetBrowserPanelWidget::ClearContentDiskWatchPaths()
{
	if (m_content_disk_watcher_ == nullptr)
	{
		return;
	}

	const QStringList WatchedPaths = m_content_disk_watcher_->directories();
	for (const QString& Path : WatchedPaths)
	{
		m_content_disk_watcher_->removePath(Path);
	}

	QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
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

	RestoreFolderTreeSelection();
}

void AssetBrowserPanelWidget::ApplyFilters()
{
	std::vector<FAssetBrowserListItem> FilteredItems;
	FilteredItems.reserve(m_all_entries_.size());

	const bool bUseNoneFilter =
		m_type_filter_widget_ != nullptr && m_type_filter_widget_->IsNoneMode();
	const QString SearchLower = m_search_text_.trimmed().toLower();
	const std::vector<std::string>& ActiveFolderPaths = GetActiveFolderFilterPaths();
	QSet<QString> AddedFolderPaths;

	if (bUseNoneFilter)
	{
		for (const std::string& SelectedFolderPath : ActiveFolderPaths)
		{
			if (QTreeWidgetItem* FolderItem = FindFolderTreeItemByPath(SelectedFolderPath))
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
					if (AddedFolderPaths.contains(FolderPath))
					{
						continue;
					}

					if (!SearchLower.isEmpty() && !FolderName.toLower().contains(SearchLower)
						&& !FolderPath.toLower().contains(SearchLower))
					{
						continue;
					}

					AddedFolderPaths.insert(FolderPath);
					FAssetBrowserListItem FolderListItem;
					FolderListItem.bIsFolder = true;
					FolderListItem.FolderPath = FolderPath.toStdString();
					FolderListItem.Entry.ObjectName = FolderName.toStdString();
					FolderListItem.Entry.AssetPath = FolderListItem.FolderPath;
					FilteredItems.push_back(std::move(FolderListItem));
				}
			}
		}
	}

	for (const FAssetRegistryEntry& Entry : m_all_entries_)
	{
		if (bUseNoneFilter)
		{
			if (!IsAssetDirectChildOfAnySelectedFolder(Entry, ActiveFolderPaths))
			{
				continue;
			}
		}
		else if (!IsAssetInAnySelectedFolder(Entry, ActiveFolderPaths))
		{
			continue;
		}

		if (!bUseNoneFilter && m_type_filter_widget_ != nullptr
			&& !m_type_filter_widget_->MatchesAssetType(Entry.Type))
		{
			continue;
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
	static_cast<AssetBrowserListView*>(m_asset_grid_)->DismissItemHover();
	m_asset_grid_->clearSelection();
	m_asset_grid_->setCurrentIndex(QModelIndex());
	m_asset_grid_->viewport()->setProperty("assetDragSourceRow", kAssetDragSourceNone);
	UpdateGridStatusLabel();

	for (int Row = 0; Row < m_list_model_->rowCount(); ++Row)
	{
		const FAssetBrowserListItem* Item = m_list_model_->GetItemAt(Row);
		if (Item != nullptr && !Item->bIsFolder)
		{
			RequestThumbnailForRow(Row, Row == 0);
		}
	}
}

void AssetBrowserPanelWidget::UpdateGridStatusLabel()
{
	if (m_status_label_ == nullptr || m_list_model_ == nullptr)
	{
		return;
	}

	const int TotalCount = m_list_model_->rowCount();
	const int SelectedCount =
		m_asset_grid_ != nullptr && m_asset_grid_->selectionModel() != nullptr
		? m_asset_grid_->selectionModel()->selectedIndexes().size()
		: 0;
	if (SelectedCount > 1)
	{
		m_status_label_->setText(tr("%1 items (%2 selected)").arg(TotalCount).arg(SelectedCount));
	}
	else
	{
		m_status_label_->setText(tr("%1 items").arg(TotalCount));
	}
}

void AssetBrowserPanelWidget::OnGridSelectionChanged()
{
	UpdateGridStatusLabel();
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
	SyncSelectedFolderPathsFromTree();
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

		m_type_filter_widget_->SetAllTypesMode();
	}
	else if (!bHasSearchText && m_b_can_auto_switch_to_none_on_search_clear_)
	{
		m_b_can_auto_switch_to_none_on_search_clear_ = false;
		m_b_can_auto_switch_to_all_types_on_search_ = true;

		m_type_filter_widget_->SetNoneMode();
	}

	ApplyFilters();
}

void AssetBrowserPanelWidget::OnTypeFilterChanged()
{
	ApplyFilters();
}

void AssetBrowserPanelWidget::OnFolderTreeContextMenuRequested(const QPoint& InPos)
{
	QTreeWidgetItem* Item = m_folder_tree_->itemAt(InPos);
	if (Item == nullptr)
	{
		Item = m_folder_tree_->currentItem();
	}

	if (Item == nullptr)
	{
		return;
	}

	const std::vector<std::string> DeletableFolderPaths = GetSelectedDeletableFolderPaths();
	const bool bIsMultiFolderSelection = DeletableFolderPaths.size() > 1;
	const std::string FolderPath = Item->data(0, Qt::UserRole).toString().toUtf8().toStdString();
	const FAssetBrowserItemCapabilities FolderCapabilities =
		GetFolderItemCapabilities(FolderPath, CanCreateFolderUnderItem(Item));

	QMenu Menu(this);
	QAction* AddFolderAction = nullptr;
	QAction* RenameAction = nullptr;
	if (!bIsMultiFolderSelection)
	{
		if (FolderCapabilities.bCanCreateSubfolder)
		{
			AddFolderAction = Menu.addAction(tr("添加新文件夹"));
		}
		if (FolderCapabilities.bCanRename)
		{
			RenameAction = Menu.addAction(tr("重命名"));
			RenameAction->setShortcut(QKeySequence(Qt::Key_F2));
		}
	}

	QAction* DeleteAction = nullptr;
	if (FolderCapabilities.bCanDelete && !DeletableFolderPaths.empty())
	{
		DeleteAction = Menu.addAction(tr("删除"));
		DeleteAction->setShortcut(QKeySequence::Delete);
	}

	QAction* Chosen = Menu.exec(m_folder_tree_->viewport()->mapToGlobal(InPos));
	if (Chosen == AddFolderAction)
	{
		OnAddNewFolderRequested();
	}
	else if (Chosen == RenameAction)
	{
		BeginRenameSelectedFolder();
	}
	else if (Chosen == DeleteAction)
	{
		DeleteSelectedFolders();
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
	m_selected_folder_paths_ = {NewFolderPath};
	m_last_registry_revision_ = 0;
	m_uasset_disk_fingerprint_ = ComputeUAssetDiskFingerprint();
	m_directory_disk_fingerprint_ = ComputeDirectoryDiskFingerprint();
	MaybeUpdateContentDiskWatchPaths();
	RefreshFromRegistry();
}

void AssetBrowserPanelWidget::OnImportAssetsRequested()
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

	const QString InitialDir = FsPathToQString(GProjectContentDirectory);
	const QStringList FilePaths = QFileDialog::getOpenFileNames(
		this,
		tr("导入"),
		InitialDir,
		tr("3D模型 (*.fbx *.obj *.gltf *.glb *.dae *.3ds *.blend);;所有文件 (*.*)"));
	if (FilePaths.isEmpty())
	{
		return;
	}

	QList<QUrl> FileUrls;
	FileUrls.reserve(FilePaths.size());
	for (const QString& FilePath : FilePaths)
	{
		FileUrls.push_back(QUrl::fromLocalFile(FilePath));
	}

	OnExternalFilesDropped(FileUrls);
}

void AssetBrowserPanelWidget::ShowGridBackgroundContextMenu(const QPoint& InPos)
{
	QTreeWidgetItem* CurrentFolderItem =
		m_folder_tree_ != nullptr ? m_folder_tree_->currentItem() : nullptr;
	const bool bCanCreateFolder = CanCreateFolderUnderItem(CurrentFolderItem);
	const bool bCanImport = CanImportIntoSelectedFolder();

	QMenu Menu(this);
	QAction* AddFolderAction = Menu.addAction(tr("新建文件夹"));
	AddFolderAction->setEnabled(bCanCreateFolder);
	QAction* ImportAction = Menu.addAction(tr("导入"));
	ImportAction->setEnabled(bCanImport);

	QAction* Chosen = Menu.exec(m_asset_grid_->viewport()->mapToGlobal(InPos));
	if (Chosen == AddFolderAction)
	{
		OnAddNewFolderRequested();
	}
	else if (Chosen == ImportAction)
	{
		OnImportAssetsRequested();
	}
}

void AssetBrowserPanelWidget::OnGridContextMenuRequested(const QPoint& InPos)
{
	const QModelIndex Index = m_asset_grid_->indexAt(InPos);
	if (!Index.isValid())
	{
		ShowGridBackgroundContextMenu(InPos);
		return;
	}

	if (m_asset_grid_->selectionModel() != nullptr && !m_asset_grid_->selectionModel()->isSelected(Index))
	{
		m_asset_grid_->selectionModel()->clearSelection();
		m_asset_grid_->selectionModel()->select(
			Index,
			QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		m_asset_grid_->selectionModel()->setCurrentIndex(Index, QItemSelectionModel::Current);
	}

	const FAssetBrowserGridSelectionState SelectionState = BuildGridSelectionState();
	if (SelectionState.Items.empty())
	{
		return;
	}

	const FAssetBrowserItemCapabilities& Capabilities = SelectionState.Capabilities;
	const bool bIsHomogeneousAssets =
		!SelectionState.bIsMixedTypeSelection && SelectionState.AssetCount > 0;

	QMenu Menu(this);
	QAction* ShowInExplorerAction = nullptr;
	QAction* CopyPathAction = nullptr;
	QAction* RenameAction = nullptr;
	QAction* DuplicateAction = nullptr;
	QAction* ReimportAction = nullptr;

	if (Capabilities.bCanShowInExplorer)
	{
		ShowInExplorerAction = Menu.addAction(tr("在资源管理器中显示"));
	}
	if (Capabilities.bCanCopySoftObjectPath)
	{
		CopyPathAction = Menu.addAction(tr("复制 SoftObjectPath"));
	}
	if (Capabilities.bCanDuplicate)
	{
		DuplicateAction = Menu.addAction(tr("创建副本"));
	}
	if (Capabilities.bCanRename)
	{
		RenameAction = Menu.addAction(tr("重命名"));
		RenameAction->setShortcut(QKeySequence(Qt::Key_F2));
	}
	if (Capabilities.bCanReimport)
	{
		ReimportAction = Menu.addAction(tr("Reimport"));
	}

	QAction* DeleteAction = nullptr;
	if (Capabilities.bCanDelete)
	{
		DeleteAction = Menu.addAction(tr("删除"));
		DeleteAction->setShortcut(QKeySequence::Delete);
	}

	if (Menu.isEmpty())
	{
		return;
	}

	const QString SoftPath = Index.data(static_cast<int>(EAssetListRole::SoftObjectPath)).toString();
	const QString UAssetPath = Index.data(static_cast<int>(EAssetListRole::UAssetFilePath)).toString();

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
		DuplicateSelectedAssets();
	}
	else if (Chosen == RenameAction)
	{
		BeginRenameSelectedGridItem();
	}
	else if (Chosen == ReimportAction)
	{
		if (SelectionState.bIsMultiSelection && bIsHomogeneousAssets)
		{
			ReimportSelectedAssets();
		}
		else
		{
			const FAssetBrowserListItem* Item = m_list_model_->GetItemAt(Index.row());
			if (Item == nullptr || Item->Entry.SourceFile.empty())
			{
				QMessageBox::warning(this, tr("Reimport"), tr("该资产缺少 SourceFile，无法 Reimport。"));
				return;
			}

			std::string ErrorMessage;
			if (!UMeshImportFactory::Reimport(SoftPath.toStdString(), Item->Entry.SourceFile, &ErrorMessage))
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
	else if (Chosen == DeleteAction)
	{
		DeleteSelectedGridItems();
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
