#pragma once

#include <QWidget>

class QLineEdit;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

class GameApp;
class AActor;

class WorldContentPanelWidget final : public QWidget
{
	Q_OBJECT

public:
	explicit WorldContentPanelWidget(GameApp* InGameApp, QWidget* InParent = nullptr);

	void RefreshFromScene();

private:
	void BuildUi();
	void RebuildActorTree();
	void SyncTreeSelection();
	void PopulateTreeItem(QTreeWidgetItem* InParentItem, const AActor* InActor);
	AActor* FindAttachmentParentActor(const AActor* InActor) const;
	bool ActorMatchesFilter(const AActor* InActor) const;
	void OnTreeItemClicked(QTreeWidgetItem* InItem, int InColumn);
	void OnTreeContextMenuRequested(const QPoint& InPos);
	void OnDeleteSelectedActorRequested();
	void OnSearchTextChanged(const QString& InText);

	GameApp* m_game_app_ = nullptr;
	QLineEdit* m_search_edit_ = nullptr;
	QTreeWidget* m_actor_tree_ = nullptr;
	QLabel* m_status_label_ = nullptr;
	uint32_t m_last_scene_revision_ = 0;
	uint64_t m_last_selected_actor_id_ = 0;
	bool m_is_tree_refreshing_ = false;
};
