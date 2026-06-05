#include "ui/DetailPanelWidget.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <algorithm>

#include "app/GameApp.h"
#include "ui/DraggableDoubleSpinBox.h"
#include "world/Actor.h"

DetailPanelWidget::DetailPanelWidget(GameApp* InGameApp, QWidget* InParent)
	: QWidget(InParent)
	, m_game_app_(InGameApp)
{
	BuildUi();
	RefreshFromSelection();
}

void DetailPanelWidget::BuildUi()
{
	auto* RootLayout = new QVBoxLayout(this);
	RootLayout->setContentsMargins(6, 6, 6, 6);
	RootLayout->setSpacing(6);

	m_header_label_ = new QLabel(tr("未选中 Actor"), this);
	m_header_label_->setObjectName("PanelHeaderLabel");
	m_header_label_->setWordWrap(true);
	m_header_label_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	m_header_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
	RootLayout->addWidget(m_header_label_);

	m_search_edit_ = new QLineEdit(this);
	m_search_edit_->setPlaceholderText(tr("搜索属性..."));
	m_search_edit_->setClearButtonEnabled(true);
	RootLayout->addWidget(m_search_edit_);

	m_transform_group_ = new QGroupBox(tr("Transform"), this);
	auto* TransformLayout = new QFormLayout(m_transform_group_);

	m_position_x_spin_ = CreateSpinBox(-100000.0, 100000.0, 0.0);
	m_position_y_spin_ = CreateSpinBox(-100000.0, 100000.0, 0.0);
	m_position_z_spin_ = CreateSpinBox(-100000.0, 100000.0, 0.0);
	TransformLayout->addRow(tr("Location X"), m_position_x_spin_);
	TransformLayout->addRow(tr("Location Y"), m_position_y_spin_);
	TransformLayout->addRow(tr("Location Z"), m_position_z_spin_);

	m_rotation_pitch_spin_ = CreateSpinBox(-360.0, 360.0, 0.0);
	m_rotation_yaw_spin_ = CreateSpinBox(-360.0, 360.0, 0.0);
	m_rotation_roll_spin_ = CreateSpinBox(-360.0, 360.0, 0.0);
	TransformLayout->addRow(tr("Rotation Pitch"), m_rotation_pitch_spin_);
	TransformLayout->addRow(tr("Rotation Yaw"), m_rotation_yaw_spin_);
	TransformLayout->addRow(tr("Rotation Roll"), m_rotation_roll_spin_);

	m_scale_x_spin_ = CreateSpinBox(0.001, 1000.0, 1.0);
	m_scale_y_spin_ = CreateSpinBox(0.001, 1000.0, 1.0);
	m_scale_z_spin_ = CreateSpinBox(0.001, 1000.0, 1.0);
	TransformLayout->addRow(tr("Scale X"), m_scale_x_spin_);
	TransformLayout->addRow(tr("Scale Y"), m_scale_y_spin_);
	TransformLayout->addRow(tr("Scale Z"), m_scale_z_spin_);

	RootLayout->addWidget(m_transform_group_);

	m_aabb_debug_checkbox_ = new QCheckBox(tr("显示 Mesh Actor AABB 线框"), this);
	m_aabb_debug_checkbox_->setEnabled(false);
	RootLayout->addWidget(m_aabb_debug_checkbox_);

	m_obb_debug_checkbox_ = new QCheckBox(tr("显示 Mesh Actor OBB 线框"), this);
	m_obb_debug_checkbox_->setEnabled(false);
	RootLayout->addWidget(m_obb_debug_checkbox_);

	m_section_bounds_debug_checkbox_ = new QCheckBox(tr("显示 Mesh Actor SectionBounds 线框"), this);
	m_section_bounds_debug_checkbox_->setEnabled(false);
	RootLayout->addWidget(m_section_bounds_debug_checkbox_);

	RootLayout->addStretch(1);

	const auto ConnectSpin = [this](DraggableDoubleSpinBox* InSpin)
	{
		if (InSpin == nullptr)
		{
			return;
		}
		connect(InSpin, &DraggableDoubleSpinBox::valueChanged, this, &DetailPanelWidget::OnTransformFieldChanged);
		connect(InSpin, &DraggableDoubleSpinBox::editingFinished, this, &DetailPanelWidget::OnTransformEditingFinished);
	};
	ConnectSpin(m_position_x_spin_);
	ConnectSpin(m_position_y_spin_);
	ConnectSpin(m_position_z_spin_);
	ConnectSpin(m_rotation_pitch_spin_);
	ConnectSpin(m_rotation_yaw_spin_);
	ConnectSpin(m_rotation_roll_spin_);
	ConnectSpin(m_scale_x_spin_);
	ConnectSpin(m_scale_y_spin_);
	ConnectSpin(m_scale_z_spin_);

	connect(m_search_edit_, &QLineEdit::textChanged, this, &DetailPanelWidget::OnSearchTextChanged);
	connect(m_aabb_debug_checkbox_, &QCheckBox::toggled, this, &DetailPanelWidget::OnAabbDebugToggled);
	connect(m_obb_debug_checkbox_, &QCheckBox::toggled, this, &DetailPanelWidget::OnObbDebugToggled);
	connect(
		m_section_bounds_debug_checkbox_,
		&QCheckBox::toggled,
		this,
		&DetailPanelWidget::OnSectionBoundsDebugToggled);
}

void DetailPanelWidget::resizeEvent(QResizeEvent* InEvent)
{
	QWidget::resizeEvent(InEvent);
	UpdateHeaderLabelLayoutWidth();
}

void DetailPanelWidget::UpdateHeaderLabelLayoutWidth()
{
	if (m_header_label_ == nullptr)
	{
		return;
	}

	const QMargins Margins = (layout() != nullptr)
		? layout()->contentsMargins()
		: QMargins{};
	const int AvailableWidth = std::max(0, width() - Margins.left() - Margins.right());
	if (AvailableWidth <= 0)
	{
		return;
	}

	m_header_label_->setMaximumWidth(AvailableWidth);
	m_header_label_->setMinimumWidth(0);
}

DraggableDoubleSpinBox* DetailPanelWidget::CreateSpinBox(double InMin, double InMax, double InValue, int InDecimals)
{
	auto* SpinBox = new DraggableDoubleSpinBox(this);
	SpinBox->setRange(InMin, InMax);
	SpinBox->setDecimals(InDecimals);
	SpinBox->setValue(InValue);
	SpinBox->setSingleStep(0.1);
	SpinBox->SetDragSensitivity(1.0);
	SpinBox->setKeyboardTracking(false);
	SpinBox->setEnabled(false);
	return SpinBox;
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
			m_is_syncing_controls_ = true;
			m_aabb_debug_checkbox_->setChecked(false);
			m_is_syncing_controls_ = false;
			if (m_game_app_ != nullptr)
			{
				m_game_app_->SetSelectedActorAabbDebugEnabled(false);
			}
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
			m_is_syncing_controls_ = true;
			m_obb_debug_checkbox_->setChecked(false);
			m_is_syncing_controls_ = false;
			if (m_game_app_ != nullptr)
			{
				m_game_app_->SetSelectedActorObbDebugEnabled(false);
			}
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
			m_is_syncing_controls_ = true;
			m_section_bounds_debug_checkbox_->setChecked(false);
			m_is_syncing_controls_ = false;
			if (m_game_app_ != nullptr)
			{
				m_game_app_->SetSelectedActorSectionBoundsDebugEnabled(false);
			}
		}
		else
		{
			SyncSectionBoundsDebugCheckbox();
		}
	}

	for (DraggableDoubleSpinBox* Spin :
		{m_position_x_spin_, m_position_y_spin_, m_position_z_spin_, m_rotation_pitch_spin_,
			m_rotation_yaw_spin_, m_rotation_roll_spin_, m_scale_x_spin_, m_scale_y_spin_,
			m_scale_z_spin_})
	{
		if (Spin != nullptr)
		{
			Spin->setEnabled(bHasSelection);
		}
	}

	if (!bHasSelection)
	{
		m_header_label_->setText(tr("未选中 Actor"));
		UpdateHeaderLabelLayoutWidth();
		PopulateFromActorTransform(FActorTransform::Identity());
		return;
	}

	const QString HeaderText =
		tr("%1 (%2)")
			.arg(QString::fromStdString(SelectedActor->GetObjectName()))
			.arg(QString::fromStdString(SelectedActor->GetClass().GetTypeName()));
	m_header_label_->setText(HeaderText);
	UpdateHeaderLabelLayoutWidth();
	PopulateFromActorTransform(SelectedActor->GetActorTransform());
}

void DetailPanelWidget::PopulateFromActorTransform(const FActorTransform& InTransform)
{
	m_is_syncing_controls_ = true;
	if (m_position_x_spin_ != nullptr)
	{
		m_position_x_spin_->setValue(InTransform.Position.X);
		m_position_y_spin_->setValue(InTransform.Position.Y);
		m_position_z_spin_->setValue(InTransform.Position.Z);
	}
	if (m_rotation_pitch_spin_ != nullptr)
	{
		m_rotation_pitch_spin_->setValue(InTransform.Rotation.Pitch);
		m_rotation_yaw_spin_->setValue(InTransform.Rotation.Yaw);
		m_rotation_roll_spin_->setValue(InTransform.Rotation.Roll);
	}
	if (m_scale_x_spin_ != nullptr)
	{
		m_scale_x_spin_->setValue(InTransform.Scale.X);
		m_scale_y_spin_->setValue(InTransform.Scale.Y);
		m_scale_z_spin_->setValue(InTransform.Scale.Z);
	}
	m_is_syncing_controls_ = false;
}

FActorTransform DetailPanelWidget::BuildTransformFromControls() const
{
	FActorTransform Result;
	if (m_position_x_spin_ != nullptr)
	{
		Result.Position.X = static_cast<float>(m_position_x_spin_->value());
		Result.Position.Y = static_cast<float>(m_position_y_spin_->value());
		Result.Position.Z = static_cast<float>(m_position_z_spin_->value());
	}
	if (m_rotation_pitch_spin_ != nullptr)
	{
		Result.Rotation.Pitch = static_cast<float>(m_rotation_pitch_spin_->value());
		Result.Rotation.Yaw = static_cast<float>(m_rotation_yaw_spin_->value());
		Result.Rotation.Roll = static_cast<float>(m_rotation_roll_spin_->value());
	}
	if (m_scale_x_spin_ != nullptr)
	{
		Result.Scale.X = static_cast<float>(m_scale_x_spin_->value());
		Result.Scale.Y = static_cast<float>(m_scale_y_spin_->value());
		Result.Scale.Z = static_cast<float>(m_scale_z_spin_->value());
	}
	return Result;
}

void DetailPanelWidget::CommitTransformFromControls()
{
	if (m_is_syncing_controls_ || m_game_app_ == nullptr)
	{
		return;
	}

	if (m_game_app_->GetSelectedActorObjectId() == 0)
	{
		return;
	}

	m_game_app_->SetSelectedActorTransform(BuildTransformFromControls(), true);
}

void DetailPanelWidget::OnTransformFieldChanged(double InValue)
{
	(void)InValue;
	CommitTransformFromControls();
}

void DetailPanelWidget::OnTransformEditingFinished()
{
	CommitTransformFromControls();
}

void DetailPanelWidget::OnSearchTextChanged(const QString& InText)
{
	if (m_transform_group_ == nullptr)
	{
		return;
	}

	const QString FilterText = InText.trimmed();
	if (FilterText.isEmpty())
	{
		m_transform_group_->setVisible(true);
		return;
	}

	const bool bShowTransform =
		tr("Transform").contains(FilterText, Qt::CaseInsensitive)
		|| tr("Location").contains(FilterText, Qt::CaseInsensitive)
		|| tr("Rotation").contains(FilterText, Qt::CaseInsensitive)
		|| tr("Scale").contains(FilterText, Qt::CaseInsensitive)
		|| QStringLiteral("X").contains(FilterText, Qt::CaseInsensitive)
		|| QStringLiteral("Y").contains(FilterText, Qt::CaseInsensitive)
		|| QStringLiteral("Z").contains(FilterText, Qt::CaseInsensitive);
	m_transform_group_->setVisible(bShowTransform);
}

void DetailPanelWidget::OnAabbDebugToggled(bool bIsChecked)
{
	if (m_is_syncing_controls_ || m_game_app_ == nullptr)
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

	m_is_syncing_controls_ = true;
	m_aabb_debug_checkbox_->setChecked(m_game_app_->IsSelectedActorAabbDebugEnabled());
	m_is_syncing_controls_ = false;
}

void DetailPanelWidget::OnObbDebugToggled(bool bIsChecked)
{
	if (m_is_syncing_controls_ || m_game_app_ == nullptr)
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

	m_is_syncing_controls_ = true;
	m_obb_debug_checkbox_->setChecked(m_game_app_->IsSelectedActorObbDebugEnabled());
	m_is_syncing_controls_ = false;
}

void DetailPanelWidget::OnSectionBoundsDebugToggled(bool bIsChecked)
{
	if (m_is_syncing_controls_ || m_game_app_ == nullptr)
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

	m_is_syncing_controls_ = true;
	m_section_bounds_debug_checkbox_->setChecked(
		m_game_app_->IsSelectedActorSectionBoundsDebugEnabled());
	m_is_syncing_controls_ = false;
}
