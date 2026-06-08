#include "ui/WorldContentPanelWidget.h"

#include <algorithm>
#include <functional>
#include <unordered_map>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "app/GameApp.h"
#include "components/SceneComponent.h"
#include "world/Actor.h"
#include "world/Level.h"

namespace
{
constexpr int kActorObjectIdRole = Qt::UserRole;
constexpr uint64_t kWorldRootItemId = 0;
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

	m_actor_tree_ = new QTreeWidget(Content);
	m_actor_tree_->setColumnCount(2);
	m_actor_tree_->setHeaderLabels({tr("Item Label"), tr("Type")});
	m_actor_tree_->setRootIsDecorated(true);
	m_actor_tree_->setAlternatingRowColors(true);
	m_actor_tree_->setSelectionMode(QAbstractItemView::SingleSelection);
	m_actor_tree_->setContextMenuPolicy(Qt::CustomContextMenu);
	m_actor_tree_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_actor_tree_->setTextElideMode(Qt::ElideMiddle);
	m_actor_tree_->setMinimumHeight(160);
	m_actor_tree_->header()->setStretchLastSection(true);
	m_actor_tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_actor_tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	m_actor_tree_->header()->setMinimumSectionSize(72);
	ContentLayout->addWidget(m_actor_tree_);

	m_status_label_ = new QLabel(tr("0 actors"), Content);
	m_status_label_->setObjectName("SecondaryLabel");
	ContentLayout->addWidget(m_status_label_);

	connect(m_search_edit_, &QLineEdit::textChanged, this, &WorldContentPanelWidget::OnSearchTextChanged);
	connect(
		m_actor_tree_,
		&QTreeWidget::itemClicked,
		this,
		&WorldContentPanelWidget::OnTreeItemClicked);
	connect(
		m_actor_tree_,
		&QTreeWidget::customContextMenuRequested,
		this,
		&WorldContentPanelWidget::OnTreeContextMenuRequested);

	RefreshScrollContentGeometry();
}

void WorldContentPanelWidget::RefreshFromScene()
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	const uint32_t SceneRevision = m_game_app_->GetSceneRevision();
	const uint64_t SelectedActorId = m_game_app_->GetSelectedActorObjectId();
	if (SceneRevision == m_last_scene_revision_ && m_actor_tree_->topLevelItemCount() > 0)
	{
		if (SelectedActorId != m_last_selected_actor_id_)
		{
			m_last_selected_actor_id_ = SelectedActorId;
			SyncTreeSelection();
		}
		return;
	}

	m_last_scene_revision_ = SceneRevision;
	m_last_selected_actor_id_ = SelectedActorId;
	RebuildActorTree();
}

void WorldContentPanelWidget::SyncTreeSelection()
{
	if (m_actor_tree_ == nullptr || m_game_app_ == nullptr)
	{
		return;
	}

	const uint64_t SelectedActorId = m_game_app_->GetSelectedActorObjectId();
	std::function<void(QTreeWidgetItem*)> SelectIfMatches;
	SelectIfMatches = [&](QTreeWidgetItem* InItem)
	{
		if (InItem == nullptr)
		{
			return;
		}
		const qulonglong ItemActorId = InItem->data(0, kActorObjectIdRole).toULongLong();
		if (ItemActorId == static_cast<qulonglong>(SelectedActorId))
		{
			m_actor_tree_->setCurrentItem(InItem);
			return;
		}
		for (int ChildIndex = 0; ChildIndex < InItem->childCount(); ++ChildIndex)
		{
			SelectIfMatches(InItem->child(ChildIndex));
		}
	};

	for (int TopIndex = 0; TopIndex < m_actor_tree_->topLevelItemCount(); ++TopIndex)
	{
		SelectIfMatches(m_actor_tree_->topLevelItem(TopIndex));
	}

	const ULevel* ActiveLevel = m_game_app_->GetActiveLevel();
	const size_t ActorCount = (ActiveLevel != nullptr) ? ActiveLevel->GetActorCount() : 0;
	const int SelectedCount = (SelectedActorId != 0) ? 1 : 0;
	if (m_status_label_ != nullptr)
	{
		m_status_label_->setText(
			tr("%1 actors (%2 selected)").arg(static_cast<int>(ActorCount)).arg(SelectedCount));
	}
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

	const size_t ActorCount = ActorById.size();
	const uint64_t SelectedActorId = m_game_app_->GetSelectedActorObjectId();
	const int SelectedCount = (SelectedActorId != 0) ? 1 : 0;
	m_status_label_->setText(
		tr("%1 actors (%2 selected)").arg(static_cast<int>(ActorCount)).arg(SelectedCount));

	SyncTreeSelection();

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
	if (ActorObjectId == 0 || ActorObjectId == kWorldRootItemId)
	{
		m_game_app_->SelectActor(0);
	}
	else
	{
		m_game_app_->SelectActor(static_cast<uint64_t>(ActorObjectId));
	}

	m_last_selected_actor_id_ = m_game_app_->GetSelectedActorObjectId();
	SyncTreeSelection();
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
		m_game_app_->SelectActor(static_cast<uint64_t>(ActorObjectId));
		m_last_selected_actor_id_ = m_game_app_->GetSelectedActorObjectId();
		SyncTreeSelection();
	}

	QMenu ContextMenu(this);
	QAction* DeleteAction = ContextMenu.addAction(tr("删除"));
	DeleteAction->setEnabled(m_game_app_->GetSelectedActorObjectId() != 0);
	connect(
		DeleteAction,
		&QAction::triggered,
		this,
		&WorldContentPanelWidget::OnDeleteSelectedActorRequested);
	ContextMenu.exec(m_actor_tree_->viewport()->mapToGlobal(InPos));
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
		m_last_selected_actor_id_ = 0;
		RefreshFromScene();
	}
}

void WorldContentPanelWidget::OnSearchTextChanged(const QString& InText)
{
	(void)InText;
	m_last_scene_revision_ = 0;
	RebuildActorTree();
}
