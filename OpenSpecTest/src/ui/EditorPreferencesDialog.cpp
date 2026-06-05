#include "ui/EditorPreferencesDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QVBoxLayout>

EditorPreferencesDialog::EditorPreferencesDialog(const FEditorPreferences& InInitialPreferences, QWidget* InParent)
	: QDialog(InParent)
{
	setWindowTitle(tr("编辑器偏好设置"));
	setModal(true);
	BuildUi();

	if (m_near_clip_spin_ != nullptr)
	{
		m_near_clip_spin_->setValue(static_cast<double>(InInitialPreferences.NearClipPlane));
	}
	if (m_far_clip_spin_ != nullptr)
	{
		m_far_clip_spin_->setValue(static_cast<double>(InInitialPreferences.FarClipPlane));
	}
	if (m_camera_speed_spin_ != nullptr)
	{
		m_camera_speed_spin_->setValue(static_cast<int>(InInitialPreferences.CameraMoveSpeed));
	}
	if (m_camera_speed_scalar_spin_ != nullptr)
	{
		m_camera_speed_scalar_spin_->setValue(static_cast<double>(InInitialPreferences.CameraSpeedScalar));
	}

	resize(420, 280);
}

void EditorPreferencesDialog::BuildUi()
{
	auto* RootLayout = new QVBoxLayout(this);

	auto* CameraGroup = new QGroupBox(tr("相机"), this);
	auto* CameraLayout = new QFormLayout(CameraGroup);

	m_near_clip_spin_ = new QDoubleSpinBox(CameraGroup);
	m_near_clip_spin_->setRange(0.01, 10000.0);
	m_near_clip_spin_->setDecimals(3);
	m_near_clip_spin_->setSingleStep(0.1);
	CameraLayout->addRow(tr("近裁切距离"), m_near_clip_spin_);

	m_far_clip_spin_ = new QDoubleSpinBox(CameraGroup);
	m_far_clip_spin_->setRange(0.1, 100000.0);
	m_far_clip_spin_->setDecimals(3);
	m_far_clip_spin_->setSingleStep(1.0);
	CameraLayout->addRow(tr("远裁切距离"), m_far_clip_spin_);

	m_camera_speed_spin_ = new QSpinBox(CameraGroup);
	m_camera_speed_spin_->setRange(1, 32);
	CameraLayout->addRow(tr("相机移动速度 (Camera Speed)"), m_camera_speed_spin_);

	m_camera_speed_scalar_spin_ = new QDoubleSpinBox(CameraGroup);
	m_camera_speed_scalar_spin_->setRange(0.1, 10.0);
	m_camera_speed_scalar_spin_->setDecimals(1);
	m_camera_speed_scalar_spin_->setSingleStep(0.1);
	CameraLayout->addRow(tr("相机速度倍率 (Camera Speed Scalar)"), m_camera_speed_scalar_spin_);

	RootLayout->addWidget(CameraGroup);

	auto* ButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(ButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(ButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	RootLayout->addWidget(ButtonBox);
}

FEditorPreferences EditorPreferencesDialog::GetPreferences() const
{
	FEditorPreferences Result;
	if (m_near_clip_spin_ != nullptr)
	{
		Result.NearClipPlane = static_cast<float>(m_near_clip_spin_->value());
	}
	if (m_far_clip_spin_ != nullptr)
	{
		Result.FarClipPlane = static_cast<float>(m_far_clip_spin_->value());
	}
	if (m_camera_speed_spin_ != nullptr)
	{
		Result.CameraMoveSpeed = static_cast<float>(m_camera_speed_spin_->value());
	}
	if (m_camera_speed_scalar_spin_ != nullptr)
	{
		Result.CameraSpeedScalar = static_cast<float>(m_camera_speed_scalar_spin_->value());
	}
	return Result;
}
