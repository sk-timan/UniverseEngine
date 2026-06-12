#include "ui/DetailPanelWidget.h"

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <algorithm>

#include "app/GameApp.h"
#include "components/SceneComponent.h"
#include "ui/PropertyDetailsBuilder.h"
#include "world/Actor.h"

DetailPanelWidget::DetailPanelWidget(GameApp* InGameApp, QWidget* InParent)
	: ScrollablePanelWidget(InParent)
	, m_game_app_(InGameApp)
{
	BuildUi();
	RefreshFromSelection();
}

void DetailPanelWidget::BuildUi()
{
	QWidget* Content = GetContentWidget();
	QVBoxLayout* ContentLayout = GetContentLayout();

	m_header_label_ = new QLabel(tr("未选中 Actor"), Content);
	m_header_label_->setObjectName("PanelHeaderLabel");
	m_header_label_->setWordWrap(true);
	m_header_label_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	m_header_label_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
	ContentLayout->addWidget(m_header_label_);

	m_search_edit_ = new QLineEdit(Content);
	m_search_edit_->setPlaceholderText(tr("搜索属性..."));
	m_search_edit_->setClearButtonEnabled(true);
	ContentLayout->addWidget(m_search_edit_);

	m_reflection_properties_host_ = new QWidget(Content);
	m_reflection_properties_layout_ = new QVBoxLayout(m_reflection_properties_host_);
	m_reflection_properties_layout_->setContentsMargins(0, 0, 0, 0);
	ContentLayout->addWidget(m_reflection_properties_host_);

	m_aabb_debug_checkbox_ = new QCheckBox(tr("显示 Mesh Actor AABB 线框"), Content);
	m_aabb_debug_checkbox_->setEnabled(false);
	ContentLayout->addWidget(m_aabb_debug_checkbox_);

	m_obb_debug_checkbox_ = new QCheckBox(tr("显示 Mesh Actor OBB 线框"), Content);
	m_obb_debug_checkbox_->setEnabled(false);
	ContentLayout->addWidget(m_obb_debug_checkbox_);

	m_section_bounds_debug_checkbox_ =
		new QCheckBox(tr("显示 Mesh Actor SectionBounds 线框"), Content);
	m_section_bounds_debug_checkbox_->setEnabled(false);
	ContentLayout->addWidget(m_section_bounds_debug_checkbox_);

	connect(m_search_edit_, &QLineEdit::textChanged, this, &DetailPanelWidget::OnSearchTextChanged);
	connect(m_aabb_debug_checkbox_, &QCheckBox::toggled, this, &DetailPanelWidget::OnAabbDebugToggled);
	connect(m_obb_debug_checkbox_, &QCheckBox::toggled, this, &DetailPanelWidget::OnObbDebugToggled);
	connect(
		m_section_bounds_debug_checkbox_,
		&QCheckBox::toggled,
		this,
		&DetailPanelWidget::OnSectionBoundsDebugToggled);

	RefreshScrollContentGeometry();
}

void DetailPanelWidget::resizeEvent(QResizeEvent* InEvent)
{
	ScrollablePanelWidget::resizeEvent(InEvent);
	UpdateHeaderLabelLayoutWidth();
}

void DetailPanelWidget::UpdateHeaderLabelLayoutWidth()
{
	if (m_header_label_ == nullptr)
	{
		return;
	}

	const QMargins Margins = (GetContentLayout() != nullptr)
		? GetContentLayout()->contentsMargins()
		: QMargins{};
	const int ViewportWidth = GetContentViewportWidth();
	const int AvailableWidth = std::max(0, ViewportWidth - Margins.left() - Margins.right());
	if (AvailableWidth <= 0)
	{
		return;
	}

	m_header_label_->setMaximumWidth(AvailableWidth);
	m_header_label_->setMinimumWidth(0);
}

void DetailPanelWidget::RebuildReflectionProperties()
{
	if (m_reflection_properties_layout_ == nullptr || m_game_app_ == nullptr)
	{
		return;
	}

	while (QLayoutItem* Item = m_reflection_properties_layout_->takeAt(0))
	{
		if (QWidget* Widget = Item->widget())
		{
			Widget->deleteLater();
		}
		delete Item;
	}

	const AActor* SelectedActor = m_game_app_->GetSelectedActor();
	if (SelectedActor == nullptr)
	{
		return;
	}

	USceneComponent* RootComponent = SelectedActor->GetRootComponent();
	if (RootComponent == nullptr)
	{
		return;
	}

	const std::string SearchText = m_search_edit_ != nullptr ? m_search_edit_->text().toStdString() : std::string{};
	QWidget* PropertiesWidget = PropertyDetailsBuilder::BuildPropertiesWidget(
		RootComponent,
		m_reflection_properties_host_,
		SearchText,
		[this]() { OnReflectionPropertyChanged(); });
	if (PropertiesWidget != nullptr)
	{
		m_reflection_properties_layout_->addWidget(PropertiesWidget);
	}
}

void DetailPanelWidget::RefreshFromSelection()
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	const uint64_t SelectedActorId = m_game_app_->GetSelectedActorObjectId();
	const uint32_t SceneRevision = m_game_app_->GetSceneRevision();
	if (SelectedActorId == m_last_selected_actor_id_ && SceneRevision == m_last_scene_revision_)
	{
		return;
	}

	m_last_selected_actor_id_ = SelectedActorId;
	m_last_scene_revision_ = SceneRevision;
	const AActor* SelectedActor = m_game_app_->GetSelectedActor();

	const bool bHasSelection = (SelectedActor != nullptr);
	if (m_aabb_debug_checkbox_ != nullptr)
	{
		m_aabb_debug_checkbox_->setEnabled(bHasSelection);
		if (!bHasSelection)
		{
			m_aabb_debug_checkbox_->setChecked(false);
			m_game_app_->SetSelectedActorAabbDebugEnabled(false);
		}
		else
		{
			SyncAabbDebugCheckbox();
		}
	}
	if (m_obb_debug_checkbox_ != nullptr)
	{
		m_obb_debug_checkbox_->setEnabled(bHasSelection);
		if (!bHasSelection)
		{
			m_obb_debug_checkbox_->setChecked(false);
			m_game_app_->SetSelectedActorObbDebugEnabled(false);
		}
		else
		{
			SyncObbDebugCheckbox();
		}
	}
	if (m_section_bounds_debug_checkbox_ != nullptr)
	{
		m_section_bounds_debug_checkbox_->setEnabled(bHasSelection);
		if (!bHasSelection)
		{
			m_section_bounds_debug_checkbox_->setChecked(false);
			m_game_app_->SetSelectedActorSectionBoundsDebugEnabled(false);
		}
		else
		{
			SyncSectionBoundsDebugCheckbox();
		}
	}

	if (!bHasSelection)
	{
		m_header_label_->setText(tr("未选中 Actor"));
		UpdateHeaderLabelLayoutWidth();
		RebuildReflectionProperties();
		return;
	}

	const QString HeaderText =
		tr("%1 (%2)")
			.arg(QString::fromStdString(SelectedActor->GetObjectName()))
			.arg(QString::fromStdString(SelectedActor->GetClass().GetTypeName()));
	m_header_label_->setText(HeaderText);
	UpdateHeaderLabelLayoutWidth();
	RebuildReflectionProperties();
	RefreshScrollContentGeometry();
}

void DetailPanelWidget::OnSearchTextChanged(const QString& InText)
{
	(void)InText;
	RebuildReflectionProperties();
}

void DetailPanelWidget::OnReflectionPropertyChanged()
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	if (AActor* SelectedActor = m_game_app_->GetSelectedActor())
	{
		if (USceneComponent* RootComponent = SelectedActor->GetRootComponent())
		{
			RootComponent->NotifyRelativeTransformEdited();
		}
	}

	if (m_game_app_ != nullptr)
	{
		m_last_scene_revision_ = m_game_app_->GetSceneRevision();
	}
	RebuildReflectionProperties();
	RefreshScrollContentGeometry();
}

void DetailPanelWidget::OnAabbDebugToggled(bool bIsChecked)
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	m_game_app_->SetSelectedActorAabbDebugEnabled(bIsChecked);
}

void DetailPanelWidget::SyncAabbDebugCheckbox()
{
	if (m_aabb_debug_checkbox_ == nullptr || m_game_app_ == nullptr)
	{
		return;
	}

	m_aabb_debug_checkbox_->setChecked(m_game_app_->IsSelectedActorAabbDebugEnabled());
}

void DetailPanelWidget::OnObbDebugToggled(bool bIsChecked)
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	m_game_app_->SetSelectedActorObbDebugEnabled(bIsChecked);
}

void DetailPanelWidget::SyncObbDebugCheckbox()
{
	if (m_obb_debug_checkbox_ == nullptr || m_game_app_ == nullptr)
	{
		return;
	}

	m_obb_debug_checkbox_->setChecked(m_game_app_->IsSelectedActorObbDebugEnabled());
}

void DetailPanelWidget::OnSectionBoundsDebugToggled(bool bIsChecked)
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	m_game_app_->SetSelectedActorSectionBoundsDebugEnabled(bIsChecked);
}

void DetailPanelWidget::SyncSectionBoundsDebugCheckbox()
{
	if (m_section_bounds_debug_checkbox_ == nullptr || m_game_app_ == nullptr)
	{
		return;
	}

	m_section_bounds_debug_checkbox_->setChecked(
		m_game_app_->IsSelectedActorSectionBoundsDebugEnabled());
}
