#include "ui/WorldContentPanelWidget.h"

#include <algorithm>
#include <functional>
#include <unordered_map>

#include <QAction>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QSignalBlocker>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "app/GameApp.h"
#include "components/SceneComponent.h"
#include "editor/EditorActorTransform.h"
#include "world/Actor.h"
#include "world/Level.h"

namespace
{
constexpr int kActorObjectIdRole = Qt::UserRole;
constexpr uint64_t kWorldRootItemId = 0;

class WorldContentTreeWidget final : public QTreeWidget
{
public:
	explicit WorldContentTreeWidget(QWidget* InParent = nullptr)
		: QTreeWidget(InParent)
	{
	}

	std::function<bool(const QTreeWidgetItem*, const QTreeWidgetItem*)> CanDropPredicate;
	std::function<void(uint64_t, uint64_t)> DropHandler;

protected:
	void startDrag(Qt::DropActions InSupportedActions) override
	{
		(void)InSupportedActions;
		// Use CopyAction only so Qt does not remove the dragged item from the tree
		// before we rebuild hierarchy from scene data.
		QTreeWidget::startDrag(Qt::CopyAction);
	}

	void dragMoveEvent(QDragMoveEvent* InEvent) override
	{
		QTreeWidgetItem* DragItem = currentItem();
		QTreeWidgetItem* DropItem = itemAt(InEvent->position().toPoint());
		if (DragItem == nullptr || DropItem == nullptr || CanDropPredicate == nullptr
			|| !CanDropPredicate(DragItem, DropItem))
		{
			InEvent->ignore();
			return;
		}

		InEvent->acceptProposedAction();
	}

	void dropEvent(QDropEvent* InEvent) override
	{
		QTreeWidgetItem* DragItem = currentItem();
		QTreeWidgetItem* DropItem = itemAt(InEvent->position().toPoint());
		if (DragItem == nullptr || DropItem == nullptr || DropHandler == nullptr || CanDropPredicate == nullptr
			|| !CanDropPredicate(DragItem, DropItem))
		{
			InEvent->ignore();
			return;
		}

		const uint64_t ChildActorObjectId =
			static_cast<uint64_t>(DragItem->data(0, kActorObjectIdRole).toULongLong());
		const uint64_t NewParentActorObjectId =
			static_cast<uint64_t>(DropItem->data(0, kActorObjectIdRole).toULongLong());
		DropHandler(ChildActorObjectId, NewParentActorObjectId);
		InEvent->setDropAction(Qt::CopyAction);
		InEvent->accept();
	}
};
} // namespace

WorldContentPanelWidget::WorldContentPanelWidget(GameApp* InGameApp, QWidget* InParent)
	: ScrollablePanelWidget(InParent)
	, m_game_app_(InGameApp)
{
	BuildUi();
	RefreshFromScene();
}

void WorldContentPanelWidget::BuildUi()
{
	QWidget* Content = GetContentWidget();
	QVBoxLayout* ContentLayout = GetContentLayout();

	m_search_edit_ = new QLineEdit(Content);
	m_search_edit_->setPlaceholderText(tr("搜索..."));
	m_search_edit_->setClearButtonEnabled(true);
	ContentLayout->addWidget(m_search_edit_);

	m_actor_tree_ = new WorldContentTreeWidget(Content);
	m_actor_tree_->setColumnCount(2);
	m_actor_tree_->setHeaderLabels({tr("Item Label"), tr("Type")});
	m_actor_tree_->setRootIsDecorated(true);
	m_actor_tree_->setAlternatingRowColors(true);
	m_actor_tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_actor_tree_->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_actor_tree_->setContextMenuPolicy(Qt::CustomContextMenu);
	m_actor_tree_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_actor_tree_->setTextElideMode(Qt::ElideMiddle);
	m_actor_tree_->setMinimumHeight(160);
	m_actor_tree_->header()->setStretchLastSection(true);
	m_actor_tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_actor_tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	m_actor_tree_->header()->setMinimumSectionSize(72);
	m_actor_tree_->setDragEnabled(true);
	m_actor_tree_->setAcceptDrops(true);
	m_actor_tree_->setDropIndicatorShown(true);
	m_actor_tree_->setDragDropMode(QAbstractItemView::DragDrop);
	m_actor_tree_->setDefaultDropAction(Qt::CopyAction);
	m_actor_tree_->installEventFilter(this);
	ContentLayout->addWidget(m_actor_tree_);

	auto* ActorTree = static_cast<WorldContentTreeWidget*>(m_actor_tree_);
	ActorTree->CanDropPredicate = [this](const QTreeWidgetItem* InDragItem, const QTreeWidgetItem* InDropItem)
	{
		return CanDropActorOnItem(InDragItem, InDropItem);
	};
	ActorTree->DropHandler = [this](uint64_t InChildActorObjectId, uint64_t InNewParentActorObjectId)
	{
		OnActorDropRequested(InChildActorObjectId, InNewParentActorObjectId);
	};

	m_status_label_ = new QLabel(tr("0 actors"), Content);
	m_status_label_->setObjectName("SecondaryLabel");
	ContentLayout->addWidget(m_status_label_);

	m_rename_action_ = new QAction(tr("重命名"), this);
	m_rename_action_->setShortcut(QKeySequence(Qt::Key_F2));
	m_rename_action_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	addAction(m_rename_action_);

	connect(m_search_edit_, &QLineEdit::textChanged, this, &WorldContentPanelWidget::OnSearchTextChanged);
	connect(
		m_actor_tree_,
		&QTreeWidget::itemClicked,
		this,
		&WorldContentPanelWidget::OnTreeItemClicked);
	connect(
		m_actor_tree_,
		&QTreeWidget::itemSelectionChanged,
		this,
		&WorldContentPanelWidget::SyncSelectionFromTreeToGameApp);
	connect(
		m_actor_tree_,
		&QTreeWidget::itemChanged,
		this,
		&WorldContentPanelWidget::OnTreeItemChanged);
	connect(
		m_actor_tree_,
		&QTreeWidget::customContextMenuRequested,
		this,
		&WorldContentPanelWidget::OnTreeContextMenuRequested);
	connect(m_rename_action_, &QAction::triggered, this, &WorldContentPanelWidget::BeginRenameSelectedItem);

	RefreshScrollContentGeometry();
}

bool WorldContentPanelWidget::eventFilter(QObject* InWatched, QEvent* InEvent)
{
	if (InWatched == m_actor_tree_ && InEvent->type() == QEvent::KeyPress)
	{
		auto* KeyEvent = static_cast<QKeyEvent*>(InEvent);
		if (KeyEvent->key() == Qt::Key_F2)
		{
			BeginRenameSelectedItem();
			return true;
		}
	}

	return ScrollablePanelWidget::eventFilter(InWatched, InEvent);
}

std::string WorldContentPanelWidget::BuildActorHierarchyFingerprint() const
{
	if (m_game_app_ == nullptr)
	{
		return {};
	}

	const ULevel* ActiveLevel = m_game_app_->GetActiveLevel();
	if (ActiveLevel == nullptr)
	{
		return {};
	}

	std::vector<std::string> Parts;
	for (const uint64_t ActorObjectId : ActiveLevel->GetActorObjectIds())
	{
		const AActor* Actor = ActiveLevel->FindActor(ActorObjectId);
		if (Actor == nullptr || Actor->IsPendingDestroy())
		{
			continue;
		}

		const AActor* ParentActor = FindAttachmentParentActor(Actor);
		const uint64_t ParentActorObjectId = (ParentActor != nullptr) ? ParentActor->GetObjectId() : 0;
		Parts.push_back(
			std::to_string(ActorObjectId) + ":"
			+ std::to_string(ParentActorObjectId) + ":"
			+ Actor->GetObjectName());
	}

	std::sort(Parts.begin(), Parts.end());
	std::string Fingerprint;
	for (const std::string& Part : Parts)
	{
		if (!Fingerprint.empty())
		{
			Fingerprint.push_back('\n');
		}
		Fingerprint += Part;
	}
	return Fingerprint;
}

void WorldContentPanelWidget::RefreshFromScene()
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	const uint32_t SceneRevision = m_game_app_->GetSceneRevision();
	const std::vector<uint64_t> SelectedActorIds = m_game_app_->GetSelectedActorObjectIds();
	const std::string HierarchyFingerprint = BuildActorHierarchyFingerprint();
	const bool bHasTreeItems = m_actor_tree_->topLevelItemCount() > 0;
	const bool bHierarchyUnchanged =
		bHasTreeItems
		&& !HierarchyFingerprint.empty()
		&& HierarchyFingerprint == m_last_actor_hierarchy_fingerprint_;

	if (SceneRevision == m_last_scene_revision_ && bHasTreeItems)
	{
		if (SelectedActorIds != m_last_selected_actor_ids_)
		{
			m_last_selected_actor_ids_ = SelectedActorIds;
			SyncTreeSelection();
		}
		return;
	}

	if (bHierarchyUnchanged)
	{
		m_last_scene_revision_ = SceneRevision;
		if (SelectedActorIds != m_last_selected_actor_ids_)
		{
			m_last_selected_actor_ids_ = SelectedActorIds;
			SyncTreeSelection();
		}
		else
		{
			UpdateSelectionStatusLabel();
		}
		return;
	}

	m_last_scene_revision_ = SceneRevision;
	m_last_selected_actor_ids_ = SelectedActorIds;
	m_last_actor_hierarchy_fingerprint_ = HierarchyFingerprint;
	RebuildActorTree();
}

void WorldContentPanelWidget::UpdateSelectionStatusLabel()
{
	if (m_status_label_ == nullptr || m_game_app_ == nullptr)
	{
		return;
	}

	const ULevel* ActiveLevel = m_game_app_->GetActiveLevel();
	const size_t ActorCount = (ActiveLevel != nullptr) ? ActiveLevel->GetActorCount() : 0;
	const int SelectedCount = static_cast<int>(m_game_app_->GetSelectedActorCount());
	m_status_label_->setText(
		tr("%1 actors (%2 selected)").arg(static_cast<int>(ActorCount)).arg(SelectedCount));
}

void WorldContentPanelWidget::SyncTreeSelection()
{
	if (m_actor_tree_ == nullptr || m_game_app_ == nullptr || m_actor_tree_->selectionModel() == nullptr)
	{
		return;
	}

	m_is_tree_refreshing_ = true;
	const QSignalBlocker TreeSignalBlocker(m_actor_tree_);

	const std::vector<uint64_t> SelectedActorIds = m_game_app_->GetSelectedActorObjectIds();
	const uint64_t PrimaryActorId = m_game_app_->GetSelectedActorObjectId();
	QItemSelection NewSelection;
	QModelIndex PrimaryIndex;

	std::function<void(QTreeWidgetItem*)> CollectSelection;
	CollectSelection = [&](QTreeWidgetItem* InItem)
	{
		if (InItem == nullptr)
		{
			return;
		}

		const qulonglong ItemActorId = InItem->data(0, kActorObjectIdRole).toULongLong();
		if (ItemActorId != 0 && ItemActorId != kWorldRootItemId)
		{
			const bool bIsSelected = std::find(
				SelectedActorIds.begin(),
				SelectedActorIds.end(),
				static_cast<uint64_t>(ItemActorId)) != SelectedActorIds.end();
			if (bIsSelected)
			{
				const QModelIndex ItemIndex = m_actor_tree_->indexFromItem(InItem);
				if (ItemIndex.isValid())
				{
					NewSelection.select(ItemIndex, ItemIndex);
					if (static_cast<uint64_t>(ItemActorId) == PrimaryActorId)
					{
						PrimaryIndex = ItemIndex;
					}
				}
			}
		}

		for (int ChildIndex = 0; ChildIndex < InItem->childCount(); ++ChildIndex)
		{
			CollectSelection(InItem->child(ChildIndex));
		}
	};

	for (int TopIndex = 0; TopIndex < m_actor_tree_->topLevelItemCount(); ++TopIndex)
	{
		CollectSelection(m_actor_tree_->topLevelItem(TopIndex));
	}

	QItemSelectionModel* SelectionModel = m_actor_tree_->selectionModel();
	SelectionModel->select(NewSelection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	if (PrimaryIndex.isValid())
	{
		SelectionModel->setCurrentIndex(PrimaryIndex, QItemSelectionModel::Current);
	}
	else
	{
		SelectionModel->setCurrentIndex(QModelIndex(), QItemSelectionModel::Current);
	}

	m_is_tree_refreshing_ = false;
	UpdateSelectionStatusLabel();
}

void WorldContentPanelWidget::SyncSelectionFromTreeToGameApp()
{
	if (m_is_tree_refreshing_ || m_game_app_ == nullptr || m_actor_tree_ == nullptr)
	{
		return;
	}

	std::vector<uint64_t> SelectedActorIds;
	const QList<QTreeWidgetItem*> SelectedItems = m_actor_tree_->selectedItems();
	SelectedActorIds.reserve(static_cast<size_t>(SelectedItems.size()));
	for (QTreeWidgetItem* Item : SelectedItems)
	{
		if (Item == nullptr)
		{
			continue;
		}

		const qulonglong ActorObjectId = Item->data(0, kActorObjectIdRole).toULongLong();
		if (ActorObjectId == 0 || ActorObjectId == kWorldRootItemId)
		{
			continue;
		}

		SelectedActorIds.push_back(static_cast<uint64_t>(ActorObjectId));
	}

	uint64_t PrimaryActorId = 0;
	if (QTreeWidgetItem* CurrentItem = m_actor_tree_->currentItem())
	{
		const qulonglong CurrentActorObjectId = CurrentItem->data(0, kActorObjectIdRole).toULongLong();
		if (CurrentActorObjectId != 0 && CurrentActorObjectId != kWorldRootItemId)
		{
			PrimaryActorId = static_cast<uint64_t>(CurrentActorObjectId);
		}
	}

	if (SelectedActorIds.empty())
	{
		m_game_app_->SelectActor(0);
	}
	else
	{
		m_game_app_->SetActorSelection(SelectedActorIds, PrimaryActorId);
	}

	m_last_selected_actor_ids_ = m_game_app_->GetSelectedActorObjectIds();
	UpdateSelectionStatusLabel();
}

AActor* WorldContentPanelWidget::FindAttachmentParentActor(const AActor* InActor) const
{
	if (InActor == nullptr)
	{
		return nullptr;
	}

	USceneComponent* RootComponent = InActor->GetRootComponent();
	if (RootComponent == nullptr)
	{
		return nullptr;
	}

	USceneComponent* AttachParent = RootComponent->GetAttachParent();
	if (AttachParent == nullptr)
	{
		return nullptr;
	}

	AActor* ParentActor = AttachParent->GetOwnerActor();
	if (ParentActor == nullptr || ParentActor == InActor)
	{
		return nullptr;
	}

	return ParentActor;
}

bool WorldContentPanelWidget::ActorMatchesFilter(const AActor* InActor) const
{
	if (InActor == nullptr || m_search_edit_ == nullptr)
	{
		return false;
	}

	const QString FilterText = m_search_edit_->text().trimmed();
	if (FilterText.isEmpty())
	{
		return true;
	}

	const QString ActorName = QString::fromStdString(InActor->GetObjectName());
	const QString ClassName = QString::fromStdString(InActor->GetClass().GetTypeName());
	return ActorName.contains(FilterText, Qt::CaseInsensitive)
		|| ClassName.contains(FilterText, Qt::CaseInsensitive);
}

bool WorldContentPanelWidget::CanRenameTreeItem(const QTreeWidgetItem* InItem) const
{
	if (InItem == nullptr)
	{
		return false;
	}

	const qulonglong ActorObjectId = InItem->data(0, kActorObjectIdRole).toULongLong();
	return ActorObjectId != 0 && ActorObjectId != kWorldRootItemId;
}

bool WorldContentPanelWidget::CanDropActorOnItem(
	const QTreeWidgetItem* InDragItem,
	const QTreeWidgetItem* InDropItem) const
{
	if (InDragItem == nullptr || InDropItem == nullptr || InDragItem == InDropItem)
	{
		return false;
	}

	const uint64_t ChildActorObjectId =
		static_cast<uint64_t>(InDragItem->data(0, kActorObjectIdRole).toULongLong());
	if (ChildActorObjectId == 0 || ChildActorObjectId == kWorldRootItemId)
	{
		return false;
	}

	const uint64_t NewParentActorObjectId =
		static_cast<uint64_t>(InDropItem->data(0, kActorObjectIdRole).toULongLong());
	if (NewParentActorObjectId == ChildActorObjectId)
	{
		return false;
	}

	if (m_game_app_ == nullptr)
	{
		return false;
	}

	const ULevel* ActiveLevel = m_game_app_->GetActiveLevel();
	if (ActiveLevel == nullptr)
	{
		return false;
	}

	const AActor* ChildActor = ActiveLevel->FindActor(ChildActorObjectId);
	if (ChildActor == nullptr)
	{
		return false;
	}

	if (NewParentActorObjectId == 0 || NewParentActorObjectId == kWorldRootItemId)
	{
		return true;
	}

	const AActor* NewParentActor = ActiveLevel->FindActor(NewParentActorObjectId);
	if (NewParentActor == nullptr)
	{
		return false;
	}

	return !FEditorActorTransform::WouldCreateAttachmentCycle(ChildActor, NewParentActor);
}

void WorldContentPanelWidget::BeginRenameSelectedItem()
{
	if (m_actor_tree_ == nullptr)
	{
		return;
	}

	QTreeWidgetItem* CurrentItem = m_actor_tree_->currentItem();
	if (!CanRenameTreeItem(CurrentItem))
	{
		return;
	}

	m_actor_tree_->editItem(CurrentItem, 0);
}

void WorldContentPanelWidget::RebuildActorTree()
{
	if (m_actor_tree_ == nullptr || m_game_app_ == nullptr)
	{
		return;
	}

	m_is_tree_refreshing_ = true;
	m_actor_tree_->clear();

	const ULevel* ActiveLevel = m_game_app_->GetActiveLevel();
	if (ActiveLevel == nullptr)
	{
		m_status_label_->setText(tr("无活动关卡"));
		m_is_tree_refreshing_ = false;
		RefreshScrollContentGeometry();
		return;
	}

	const QString LevelLabel =
		tr("root (%1)").arg(QString::fromStdString(ActiveLevel->GetLevelId()));
	auto* WorldRootItem = new QTreeWidgetItem(m_actor_tree_);
	WorldRootItem->setText(0, LevelLabel);
	WorldRootItem->setText(1, tr("World"));
	WorldRootItem->setData(0, kActorObjectIdRole, QVariant::fromValue<qulonglong>(kWorldRootItemId));
	WorldRootItem->setFlags(WorldRootItem->flags() | Qt::ItemIsDropEnabled);
	WorldRootItem->setExpanded(true);

	std::unordered_map<uint64_t, const AActor*> ActorById;
	std::vector<const AActor*> RootActors;

	for (const uint64_t ActorObjectId : ActiveLevel->GetActorObjectIds())
	{
		const AActor* Actor = ActiveLevel->FindActor(ActorObjectId);
		if (Actor == nullptr || Actor->IsPendingDestroy())
		{
			continue;
		}
		ActorById[ActorObjectId] = Actor;
	}

	for (const auto& ActorEntry : ActorById)
	{
		const AActor* Actor = ActorEntry.second;
		AActor* ParentActor = FindAttachmentParentActor(Actor);
		if (ParentActor != nullptr
			&& ActorById.find(ParentActor->GetObjectId()) != ActorById.end())
		{
			continue;
		}
		RootActors.push_back(Actor);
	}

	auto SortActorsByName = [](const AActor* InLeft, const AActor* InRight)
	{
		if (InLeft == nullptr || InRight == nullptr)
		{
			return InLeft != nullptr;
		}
		return InLeft->GetObjectName() < InRight->GetObjectName();
	};

	std::sort(RootActors.begin(), RootActors.end(), SortActorsByName);
	for (auto* RootActor : RootActors)
	{
		if (!ActorMatchesFilter(RootActor))
		{
			continue;
		}
		PopulateTreeItem(WorldRootItem, RootActor);
	}

	(void)ActorById;
	UpdateSelectionStatusLabel();
	SyncTreeSelection();

	m_last_actor_hierarchy_fingerprint_ = BuildActorHierarchyFingerprint();
	if (m_game_app_ != nullptr)
	{
		m_last_scene_revision_ = m_game_app_->GetSceneRevision();
		m_last_selected_actor_ids_ = m_game_app_->GetSelectedActorObjectIds();
	}

	m_is_tree_refreshing_ = false;
	RefreshScrollContentGeometry();
}

void WorldContentPanelWidget::PopulateTreeItem(QTreeWidgetItem* InParentItem, const AActor* InActor)
{
	if (InParentItem == nullptr || InActor == nullptr || m_game_app_ == nullptr)
	{
		return;
	}

	const ULevel* ActiveLevel = m_game_app_->GetActiveLevel();
	if (ActiveLevel == nullptr)
	{
		return;
	}

	auto* ActorItem = new QTreeWidgetItem(InParentItem);
	ActorItem->setText(0, QString::fromStdString(InActor->GetObjectName()));
	ActorItem->setText(1, QString::fromStdString(InActor->GetClass().GetTypeName()));
	ActorItem->setData(
		0,
		kActorObjectIdRole,
		QVariant::fromValue<qulonglong>(static_cast<qulonglong>(InActor->GetObjectId())));
	ActorItem->setFlags(ActorItem->flags() | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
	ActorItem->setExpanded(true);

	std::vector<const AActor*> ChildActors;
	for (const uint64_t ActorObjectId : ActiveLevel->GetActorObjectIds())
	{
		const AActor* Candidate = ActiveLevel->FindActor(ActorObjectId);
		if (Candidate == nullptr || Candidate->IsPendingDestroy())
		{
			continue;
		}
		if (FindAttachmentParentActor(Candidate) == InActor)
		{
			ChildActors.push_back(Candidate);
		}
	}

	std::sort(
		ChildActors.begin(),
		ChildActors.end(),
		[](const AActor* InLeft, const AActor* InRight)
		{
			return InLeft->GetObjectName() < InRight->GetObjectName();
		});

	for (const AActor* ChildActor : ChildActors)
	{
		if (!ActorMatchesFilter(ChildActor))
		{
			continue;
		}
		PopulateTreeItem(ActorItem, ChildActor);
	}
}

void WorldContentPanelWidget::OnTreeItemClicked(QTreeWidgetItem* InItem, int InColumn)
{
	(void)InColumn;
	if (m_is_tree_refreshing_ || m_game_app_ == nullptr || InItem == nullptr)
	{
		return;
	}

	const qulonglong ActorObjectId = InItem->data(0, kActorObjectIdRole).toULongLong();
	if (ActorObjectId == kWorldRootItemId)
	{
		m_is_tree_refreshing_ = true;
		m_actor_tree_->clearSelection();
		InItem->setSelected(true);
		m_actor_tree_->setCurrentItem(InItem);
		m_is_tree_refreshing_ = false;
		m_game_app_->SelectActor(0);
		m_last_selected_actor_ids_.clear();
		UpdateSelectionStatusLabel();
	}
}

void WorldContentPanelWidget::OnTreeItemChanged(QTreeWidgetItem* InItem, int InColumn)
{
	(void)InColumn;
	if (m_is_tree_refreshing_ || m_is_renaming_tree_item_ || m_game_app_ == nullptr || InItem == nullptr)
	{
		return;
	}

	if (!CanRenameTreeItem(InItem))
	{
		return;
	}

	const uint64_t ActorObjectId = static_cast<uint64_t>(InItem->data(0, kActorObjectIdRole).toULongLong());
	const std::string NewName = InItem->text(0).trimmed().toStdString();
	if (NewName.empty())
	{
		m_last_scene_revision_ = 0;
		RebuildActorTree();
		return;
	}

	std::string ErrorMessage;
	if (!m_game_app_->RenameActor(ActorObjectId, NewName, &ErrorMessage))
	{
		QMessageBox::warning(
			this,
			tr("重命名失败"),
			tr("无法重命名 Actor: %1").arg(QString::fromStdString(ErrorMessage)));
		m_last_scene_revision_ = 0;
		RebuildActorTree();
		return;
	}

	m_last_scene_revision_ = 0;
	RefreshFromScene();
}

void WorldContentPanelWidget::OnTreeContextMenuRequested(const QPoint& InPos)
{
	if (m_game_app_ == nullptr || m_actor_tree_ == nullptr)
	{
		return;
	}

	QTreeWidgetItem* Item = m_actor_tree_->itemAt(InPos);
	if (Item == nullptr)
	{
		return;
	}

	const qulonglong ActorObjectId = Item->data(0, kActorObjectIdRole).toULongLong();
	if (ActorObjectId != 0 && ActorObjectId != kWorldRootItemId)
	{
		if (!Item->isSelected())
		{
			m_actor_tree_->setCurrentItem(Item);
			Item->setSelected(true);
		}
		else
		{
			m_actor_tree_->setCurrentItem(Item);
		}
		SyncSelectionFromTreeToGameApp();
	}

	QMenu ContextMenu(this);
	QAction* RenameAction = ContextMenu.addAction(tr("重命名"));
	RenameAction->setShortcut(QKeySequence(Qt::Key_F2));
	RenameAction->setEnabled(CanRenameTreeItem(Item));
	QAction* DeleteAction = ContextMenu.addAction(tr("删除"));
	DeleteAction->setEnabled(m_game_app_->GetSelectedActorCount() > 0);
	connect(
		RenameAction,
		&QAction::triggered,
		this,
		&WorldContentPanelWidget::BeginRenameSelectedItem);
	connect(
		DeleteAction,
		&QAction::triggered,
		this,
		&WorldContentPanelWidget::OnDeleteSelectedActorRequested);

	QAction* ChosenAction = ContextMenu.exec(m_actor_tree_->viewport()->mapToGlobal(InPos));
	(void)ChosenAction;
}

void WorldContentPanelWidget::OnDeleteSelectedActorRequested()
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	if (m_game_app_->DeleteSelectedActor())
	{
		m_last_scene_revision_ = 0;
		m_last_selected_actor_ids_.clear();
		RefreshFromScene();
	}
}

void WorldContentPanelWidget::OnSearchTextChanged(const QString& InText)
{
	(void)InText;
	m_last_scene_revision_ = 0;
	RebuildActorTree();
}

void WorldContentPanelWidget::OnActorDropRequested(
	uint64_t InChildActorObjectId,
	uint64_t InNewParentActorObjectId)
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	std::string ErrorMessage;
	if (!m_game_app_->ReparentActor(InChildActorObjectId, InNewParentActorObjectId, &ErrorMessage))
	{
		QMessageBox::warning(
			this,
			tr("设置父子级失败"),
			tr("无法设置 Actor 父子级: %1").arg(QString::fromStdString(ErrorMessage)));
		return;
	}

	if (m_game_app_ != nullptr)
	{
		m_last_scene_revision_ = m_game_app_->GetSceneRevision();
	}
	RebuildActorTree();

	// Rebuild again on the next event-loop turn in case Qt post-processes the drag.
	QTimer::singleShot(
		0,
		this,
		[this]()
		{
			if (m_game_app_ == nullptr)
			{
				return;
			}
			m_last_scene_revision_ = m_game_app_->GetSceneRevision();
			RebuildActorTree();
		});
}
