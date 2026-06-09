#pragma once

#include <optional>

#include <QList>
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

class AssetListModel;
class GameApp;

inline constexpr const char* kSoftObjectPathMimeType = "application/x-universeengine-softobjectpath";
inline constexpr int kAssetDragSourceNone = -1;

class AssetBrowserPanelWidget final : public QWidget
{
	Q_OBJECT

public:
	explicit AssetBrowserPanelWidget(GameApp* InGameApp, QWidget* InParent = nullptr);

	void RefreshFromRegistry();
	void ApplyInitialSplitterProportions();

protected:
	bool eventFilter(QObject* InWatched, QEvent* InEvent) override;
	void showEvent(QShowEvent* InEvent) override;

private:
	void BuildUi();
	void SetupContentDiskWatcher();
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
	QTreeWidgetItem* FindFolderTreeItemByPath(const std::string& InFolderPath) const;
	void BeginRenameSelectedFolder();
	void BeginRenameSelectedAsset();
	void CopySelectedAsset();
	void PasteCopiedAsset();
	void DuplicateSelectedAsset();
	void OnAssetDroppedToFolder(const std::string& InTargetFolderPath, const std::string& InSoftObjectPath);
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
	std::optional<FAssetRegistryEntry> m_copied_asset_entry_;
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
