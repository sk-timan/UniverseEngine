#include "ui/CameraSpeedControlWidget.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QSlider>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace
{
	constexpr int kMinCameraSpeed = 1;
	constexpr int kMaxCameraSpeed = 32;
}

CameraSpeedControlWidget::CameraSpeedControlWidget(QWidget* InParent)
	: QWidget(InParent)
{
	setAttribute(Qt::WA_StyledBackground, true);
	setAttribute(Qt::WA_NoSystemBackground, true);
	setStyleSheet(QStringLiteral("background: transparent;"));
	BuildUi();
	qApp->installEventFilter(this);
}

void CameraSpeedControlWidget::BuildUi()
{
	auto* RootLayout = new QHBoxLayout(this);
	RootLayout->setContentsMargins(8, 8, 8, 0);
	RootLayout->setSpacing(0);

	m_trigger_button_ = new QPushButton(this);
	m_trigger_button_->setObjectName("CameraSpeedTrigger");
	m_trigger_button_->setCursor(Qt::PointingHandCursor);
	m_trigger_button_->setFixedHeight(26);
	m_trigger_button_->setMinimumWidth(56);
	m_trigger_button_->setToolTip(tr("相机移动速度"));
	connect(m_trigger_button_, &QPushButton::clicked, this, &CameraSpeedControlWidget::OnTriggerClicked);
	RootLayout->addWidget(m_trigger_button_);

	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	m_popup_frame_ = new QFrame(this, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
	m_popup_frame_->setObjectName("CameraSpeedPopup");
	m_popup_frame_->setAttribute(Qt::WA_StyledBackground, true);
	m_popup_frame_->setFixedWidth(248);

	auto* PopupLayout = new QVBoxLayout(m_popup_frame_);
	PopupLayout->setContentsMargins(12, 10, 12, 12);
	PopupLayout->setSpacing(8);

	auto* SpeedTitle = new QLabel(tr("Camera Speed"), m_popup_frame_);
	SpeedTitle->setObjectName("CameraSpeedSectionLabel");

	auto* SpeedRow = new QHBoxLayout();
	m_speed_slider_ = new QSlider(Qt::Horizontal, m_popup_frame_);
	m_speed_slider_->setObjectName("CameraSpeedSlider");
	m_speed_slider_->setRange(kMinCameraSpeed, kMaxCameraSpeed);
	m_speed_slider_->setValue(m_camera_speed_);
	m_speed_slider_->setFixedHeight(18);
	connect(m_speed_slider_, &QSlider::valueChanged, this, &CameraSpeedControlWidget::OnSpeedSliderChanged);

	m_speed_value_label_ = new QLabel(m_popup_frame_);
	m_speed_value_label_->setObjectName("CameraSpeedValueLabel");
	m_speed_value_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	m_speed_value_label_->setMinimumWidth(24);

	SpeedRow->addWidget(m_speed_slider_, 1);
	SpeedRow->addWidget(m_speed_value_label_, 0);

	auto* ScalarTitle = new QLabel(tr("Camera Speed Scalar"), m_popup_frame_);
	ScalarTitle->setObjectName("CameraSpeedSectionLabel");

	m_scalar_edit_ = new QLineEdit(m_popup_frame_);
	m_scalar_edit_->setObjectName("CameraSpeedScalarEdit");
	m_scalar_edit_->setAlignment(Qt::AlignLeft);
	m_scalar_edit_->setText(QString::number(static_cast<double>(m_camera_speed_scalar_), 'f', 1));
	connect(
		m_scalar_edit_,
		&QLineEdit::editingFinished,
		this,
		&CameraSpeedControlWidget::OnScalarEditingFinished);

	PopupLayout->addWidget(SpeedTitle);
	PopupLayout->addLayout(SpeedRow);
	PopupLayout->addWidget(ScalarTitle);
	PopupLayout->addWidget(m_scalar_edit_);

	UpdateTriggerLabel();
	UpdateSpeedValueLabel();
	m_popup_frame_->hide();
}

void CameraSpeedControlWidget::SetCameraSpeed(int InSpeed)
{
	const int ClampedSpeed = std::clamp(InSpeed, kMinCameraSpeed, kMaxCameraSpeed);
	if (m_camera_speed_ == ClampedSpeed)
	{
		return;
	}

	m_camera_speed_ = ClampedSpeed;
	m_is_syncing_controls_ = true;
	if (m_speed_slider_)
	{
		m_speed_slider_->setValue(m_camera_speed_);
	}
	m_is_syncing_controls_ = false;
	UpdateTriggerLabel();
	UpdateSpeedValueLabel();
}

void CameraSpeedControlWidget::SetCameraSpeedScalar(float InScalar)
{
	const float ClampedScalar = std::clamp(InScalar, 0.1f, 10.0f);
	if (std::abs(m_camera_speed_scalar_ - ClampedScalar) < 0.0001f)
	{
		return;
	}

	m_camera_speed_scalar_ = ClampedScalar;
	m_is_syncing_controls_ = true;
	if (m_scalar_edit_)
	{
		m_scalar_edit_->setText(QString::number(static_cast<double>(m_camera_speed_scalar_), 'f', 1));
	}
	m_is_syncing_controls_ = false;
}

int CameraSpeedControlWidget::GetCameraSpeed() const
{
	return m_camera_speed_;
}

float CameraSpeedControlWidget::GetCameraSpeedScalar() const
{
	return m_camera_speed_scalar_;
}

void CameraSpeedControlWidget::UpdateTriggerLabel()
{
	if (!m_trigger_button_)
	{
		return;
	}
	m_trigger_button_->setText(QStringLiteral("%1").arg(m_camera_speed_));
}

void CameraSpeedControlWidget::UpdateSpeedValueLabel()
{
	if (!m_speed_value_label_)
	{
		return;
	}
	m_speed_value_label_->setText(QString::number(m_camera_speed_));
}

void CameraSpeedControlWidget::PositionPopup()
{
	if (!m_trigger_button_ || !m_popup_frame_)
	{
		return;
	}

	const QPoint Anchor = m_trigger_button_->mapToGlobal(QPoint(0, m_trigger_button_->height() + 2));
	m_popup_frame_->move(Anchor.x() + m_trigger_button_->width() - m_popup_frame_->width(), Anchor.y());
}

void CameraSpeedControlWidget::SetPopupVisible(bool bIsVisible)
{
	if (!m_popup_frame_)
	{
		return;
	}

	m_is_popup_visible_ = bIsVisible;
	if (bIsVisible)
	{
		PositionPopup();
		m_popup_frame_->show();
		m_popup_frame_->raise();
	}
	else
	{
		m_popup_frame_->hide();
	}
}

void CameraSpeedControlWidget::OnTriggerClicked()
{
	SetPopupVisible(!m_is_popup_visible_);
}

void CameraSpeedControlWidget::OnSpeedSliderChanged(int InValue)
{
	if (m_is_syncing_controls_)
	{
		return;
	}

	m_camera_speed_ = std::clamp(InValue, kMinCameraSpeed, kMaxCameraSpeed);
	UpdateTriggerLabel();
	UpdateSpeedValueLabel();
	emit CameraSpeedChanged(m_camera_speed_);
}

void CameraSpeedControlWidget::OnScalarEditingFinished()
{
	if (m_is_syncing_controls_ || !m_scalar_edit_)
	{
		return;
	}

	bool bOk = false;
	const double ParsedValue = m_scalar_edit_->text().trimmed().toDouble(&bOk);
	if (!bOk)
	{
		m_scalar_edit_->setText(QString::number(static_cast<double>(m_camera_speed_scalar_), 'f', 1));
		return;
	}

	const float ClampedScalar = std::clamp(static_cast<float>(ParsedValue), 0.1f, 10.0f);
	m_camera_speed_scalar_ = ClampedScalar;
	m_scalar_edit_->setText(QString::number(static_cast<double>(m_camera_speed_scalar_), 'f', 1));
	emit CameraSpeedScalarChanged(m_camera_speed_scalar_);
}

bool CameraSpeedControlWidget::eventFilter(QObject* InWatched, QEvent* InEvent)
{
	if (m_is_popup_visible_ && InEvent->type() == QEvent::MouseButtonPress)
	{
		auto* MouseEvent = dynamic_cast<QMouseEvent*>(InEvent);
		if (MouseEvent != nullptr)
		{
			const QPoint GlobalPos = MouseEvent->globalPosition().toPoint();
			const bool bHitTrigger =
				m_trigger_button_ != nullptr && m_trigger_button_->rect().contains(
					m_trigger_button_->mapFromGlobal(GlobalPos));
			const bool bHitPopup =
				m_popup_frame_ != nullptr && m_popup_frame_->isVisible() &&
				m_popup_frame_->geometry().contains(GlobalPos);
			if (!bHitTrigger && !bHitPopup)
			{
				SetPopupVisible(false);
			}
		}
	}
	return QWidget::eventFilter(InWatched, InEvent);
}
