#pragma once

#include <QWidget>

#include "asset/AssetRegistry.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QListView;
class QSplitter;
class QTreeWidget;
class QTreeWidgetItem;

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

private:
	void BuildUi();
	void RebuildFolderTree();
	void ApplyFilters();
	void RequestVisibleThumbnails();
	void RequestThumbnailForRow(int InRow, bool bHighPriority);
	void OnFolderSelectionChanged();
	void OnSearchTextChanged(const QString& InText);
	void OnTypeFilterChanged(int InIndex);
	void OnGridContextMenuRequested(const QPoint& InPos);
	void OnThumbnailReady(const QString& InCacheKey, const QImage& InImage);
	void OnGridScrolled();

	GameApp* m_game_app_ = nullptr;
	QLineEdit* m_search_edit_ = nullptr;
	QComboBox* m_type_filter_combo_ = nullptr;
	QTreeWidget* m_folder_tree_ = nullptr;
	QListView* m_asset_grid_ = nullptr;
	AssetListModel* m_list_model_ = nullptr;
	QLabel* m_status_label_ = nullptr;
	QSplitter* m_splitter_ = nullptr;

	std::vector<FAssetRegistryEntry> m_all_entries_;
	std::string m_selected_folder_path_ = "All";
	QString m_search_text_;
	QString m_type_filter_;
	uint64_t m_last_registry_revision_ = 0;
};
