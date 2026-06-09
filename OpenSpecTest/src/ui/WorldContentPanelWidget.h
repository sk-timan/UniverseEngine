#pragma once

#include <vector>

#include "ui/ScrollablePanelWidget.h"

class QLineEdit;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QAction;

class GameApp;
class AActor;

class WorldContentPanelWidget final : public ScrollablePanelWidget
{
	Q_OBJECT

public:
	explicit WorldContentPanelWidget(GameApp* InGameApp, QWidget* InParent = nullptr);

	void RefreshFromScene();

protected:
	bool eventFilter(QObject* InWatched, QEvent* InEvent) override;

private:
	void BuildUi();
	void RebuildActorTree();
	void SyncTreeSelection();
	void SyncSelectionFromTreeToGameApp();
	void UpdateSelectionStatusLabel();
	std::string BuildActorHierarchyFingerprint() const;
	void PopulateTreeItem(QTreeWidgetItem* InParentItem, const AActor* InActor);
	AActor* FindAttachmentParentActor(const AActor* InActor) const;
	bool ActorMatchesFilter(const AActor* InActor) const;
	bool CanRenameTreeItem(const QTreeWidgetItem* InItem) const;
	bool CanDropActorOnItem(const QTreeWidgetItem* InDragItem, const QTreeWidgetItem* InDropItem) const;
	void BeginRenameSelectedItem();
	void OnTreeItemClicked(QTreeWidgetItem* InItem, int InColumn);
	void OnTreeItemChanged(QTreeWidgetItem* InItem, int InColumn);
	void OnTreeContextMenuRequested(const QPoint& InPos);
	void OnDeleteSelectedActorRequested();
	void OnSearchTextChanged(const QString& InText);
	void OnActorDropRequested(uint64_t InChildActorObjectId, uint64_t InNewParentActorObjectId);

	GameApp* m_game_app_ = nullptr;
	QLineEdit* m_search_edit_ = nullptr;
	QTreeWidget* m_actor_tree_ = nullptr;
	QLabel* m_status_label_ = nullptr;
	QAction* m_rename_action_ = nullptr;
	uint32_t m_last_scene_revision_ = 0;
	std::string m_last_actor_hierarchy_fingerprint_;
	std::vector<uint64_t> m_last_selected_actor_ids_;
	bool m_is_tree_refreshing_ = false;
	bool m_is_renaming_tree_item_ = false;
};
