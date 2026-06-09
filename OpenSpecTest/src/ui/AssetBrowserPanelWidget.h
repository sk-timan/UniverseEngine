#pragma once

#include <optional>
#include <string>
#include <vector>

#include <QList>
#include <QMimeData>
#include <QUrl>
#include <QWidget>

#include "asset/AssetRegistry.h"

class QComboBox;
class QShowEvent;
class QFileSystemWatcher;
class QLabel;
class QLineEdit;
class QListView;
class QSplitter;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class QAction;

struct FAssetBrowserListItem;
class AssetListModel;
class GameApp;

inline constexpr const char* kSoftObjectPathMimeType = "application/x-universeengine-softobjectpath";
inline constexpr const char* kFolderPathMimeType = "application/x-universeengine-folderpath";
inline constexpr int kAssetDragSourceNone = -1;

inline std::vector<std::string> ExtractSoftObjectPathsFromMimeData(const QMimeData* InMimeData)
{
	std::vector<std::string> SoftPaths;
	if (InMimeData == nullptr || !InMimeData->hasFormat(kSoftObjectPathMimeType))
	{
		return SoftPaths;
	}

	const QStringList Parts = QString::fromUtf8(InMimeData->data(kSoftObjectPathMimeType))
		.split('\n', Qt::SkipEmptyParts);
	for (const QString& Part : Parts)
	{
		const std::string SoftPath = Part.trimmed().toStdString();
		if (!SoftPath.empty())
		{
			SoftPaths.push_back(SoftPath);
		}
	}
	return SoftPaths;
}

inline std::vector<std::string> ExtractFolderPathsFromMimeData(const QMimeData* InMimeData)
{
	std::vector<std::string> FolderPaths;
	if (InMimeData == nullptr || !InMimeData->hasFormat(kFolderPathMimeType))
	{
		return FolderPaths;
	}

	const QStringList Parts = QString::fromUtf8(InMimeData->data(kFolderPathMimeType))
		.split('\n', Qt::SkipEmptyParts);
	for (const QString& Part : Parts)
	{
		const std::string FolderPath = Part.trimmed().toStdString();
		if (!FolderPath.empty())
		{
			FolderPaths.push_back(FolderPath);
		}
	}
	return FolderPaths;
}

inline bool HasContentBrowserDropMimeData(const QMimeData* InMimeData)
{
	return InMimeData != nullptr
		&& (InMimeData->hasFormat(kSoftObjectPathMimeType) || InMimeData->hasFormat(kFolderPathMimeType));
}

class AssetBrowserPanelWidget final : public QWidget
{
	Q_OBJECT

public:
	explicit AssetBrowserPanelWidget(GameApp* InGameApp, QWidget* InParent = nullptr);

	void RefreshFromRegistry();
	void ApplyInitialSplitterProportions();
	bool TryHandleDeleteShortcut();
	void ClearAssetGridSelection();
	void ClearFolderTreeMultiSelection();
	void ClearAllSelections();
	bool ContainsFolderTreeWidget(const QWidget* InWidget) const;
	bool ContainsItemGridWidget(const QWidget* InWidget) const;

protected:
	bool eventFilter(QObject* InWatched, QEvent* InEvent) override;
	void showEvent(QShowEvent* InEvent) override;

private:
	void BuildUi();
	void SetupContentDiskWatcher();
	void ClearContentDiskWatchPaths();
	void UpdateContentDiskWatchPaths();
	void MaybePollContentDiskChanges();
	void ApplyContentDiskChanges();
	void RefreshFolderTreeFromDisk();
	void MaybeUpdateContentDiskWatchPaths();
	void SyncUiFromRegistry();
	void OnDebouncedContentDiskRescan();
	void RebuildFolderTree();
	void ApplyFilters();
	void RequestVisibleThumbnails();
	void RequestThumbnailForRow(int InRow, bool bHighPriority);
	void OnFolderSelectionChanged();
	void OnSearchTextChanged(const QString& InText);
	void OnTypeFilterChanged(int InIndex);
	void OnGridContextMenuRequested(const QPoint& InPos);
	void OnFolderTreeContextMenuRequested(const QPoint& InPos);
	void OnAddNewFolderRequested();
	void OnExternalFilesDropped(const QList<QUrl>& InFileUrls);
	void OnThumbnailReady(const QString& InCacheKey, const QImage& InImage);
	void OnGridScrolled();
	void OnAssetGridDoubleClicked(const QModelIndex& InIndex);
	void OnGridSelectionChanged();
	void UpdateGridStatusLabel();
	QTreeWidgetItem* FindFolderTreeItemByPath(const std::string& InFolderPath) const;
	void BeginRenameSelectedFolder();
	void BeginRenameSelectedGridFolder();
	void BeginRenameSelectedAsset();
	void CopySelectedAsset();
	void PasteCopiedAsset();
	void DuplicateSelectedAssets();
	void ReimportSelectedAssets();
	void DeleteSelectedAssets();
	void DeleteSelectedFolders();
	bool IsAssetGridFocused() const;
	bool IsFolderTreeFocused() const;
	bool IsRenderViewportFocused() const;
	bool IsAssetDeleteShortcutEnabled() const;
	void OnItemsDroppedToFolder(
		const std::string& InTargetFolderPath,
		const std::vector<std::string>& InSoftObjectPaths,
		const std::vector<std::string>& InFolderPaths);
	std::vector<const FAssetBrowserListItem*> GetSelectedAssetItems() const;
	std::vector<std::string> GetSelectedDeletableFolderPaths() const;
	std::vector<std::string> GetSelectedGridFolderPaths() const;
	std::vector<std::string> CollectSelectedFolderPathsForOperation() const;
	bool CanRenameFolderPath(const std::string& InFolderPath) const;
	bool CanDeleteFolderItem(const QTreeWidgetItem* InItem) const;
	bool RenameFolderByPath(const std::string& InFolderPath, const std::string& InNewFolderName);
	void RefreshAfterAssetDiskMutation();
	void OnFolderItemChanged(QTreeWidgetItem* InItem, int InColumn);
	void SelectFolderTreeItemByPath(const std::string& InFolderPath);
	bool CanRenameFolderItem(const QTreeWidgetItem* InItem) const;
	bool CanCreateFolderUnderItem(const QTreeWidgetItem* InItem) const;
	bool CanImportIntoSelectedFolder() const;
	std::string BuildImportAssetPath(const std::string& InModelName) const;

	GameApp* m_game_app_ = nullptr;
	QLineEdit* m_search_edit_ = nullptr;
	QComboBox* m_type_filter_combo_ = nullptr;
	QTreeWidget* m_folder_tree_ = nullptr;
	QListView* m_asset_grid_ = nullptr;
	AssetListModel* m_list_model_ = nullptr;
	QLabel* m_status_label_ = nullptr;
	QSplitter* m_splitter_ = nullptr;
	QAction* m_rename_action_ = nullptr;
	QAction* m_copy_action_ = nullptr;
	QAction* m_paste_action_ = nullptr;
	QAction* m_delete_action_ = nullptr;
	std::vector<FAssetRegistryEntry> m_copied_asset_entries_;
	QFileSystemWatcher* m_content_disk_watcher_ = nullptr;
	QTimer* m_content_rescan_timer_ = nullptr;

	std::vector<FAssetRegistryEntry> m_all_entries_;
	std::string m_selected_folder_path_ = "All";
	QString m_search_text_;
	QString m_type_filter_;
	uint64_t m_last_registry_revision_ = 0;
	uint64_t m_uasset_disk_fingerprint_ = 0;
	uint64_t m_directory_disk_fingerprint_ = 0;
	uint32_t m_watched_directory_count_ = 0;
	qint64 m_last_content_disk_poll_ms_ = 0;
	bool m_is_folder_rename_in_progress_ = false;
	bool m_b_initial_splitter_applied_ = false;
	bool m_b_can_auto_switch_to_all_types_on_search_ = true;
	bool m_b_can_auto_switch_to_none_on_search_clear_ = false;
};
