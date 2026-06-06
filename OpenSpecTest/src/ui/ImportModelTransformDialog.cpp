#include "ui/ImportModelTransformDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

ImportModelTransformDialog::ImportModelTransformDialog(
	const QString& InModelName,
	EImportModelDialogMode InMode,
	QWidget* InParent)
	: QDialog(InParent)
	, m_mode_(InMode)
{
	setWindowTitle(InMode == EImportModelDialogMode::ImportAsset ? tr("导入资产") : tr("加载模型"));
	setModal(true);
	BuildUi(InModelName);
}

void ImportModelTransformDialog::BuildUi(const QString& InModelName)
{
	auto* RootLayout = new QVBoxLayout(this);

	const QString HintText = m_mode_ == EImportModelDialogMode::ImportAsset
		? tr("即将导入源文件并写入 Content 资产: %1\n请指定 Content 路径。").arg(InModelName)
		: tr("即将加载 uasset 到当前关卡: %1\n请设置位置、旋转（度）与缩放。").arg(InModelName);
	auto* HintLabel = new QLabel(HintText, this);
	HintLabel->setWordWrap(true);
	RootLayout->addWidget(HintLabel);

	if (m_mode_ == EImportModelDialogMode::ImportAsset)
	{
		auto* AssetPathGroup = new QGroupBox(tr("Content 资产路径"), this);
		auto* AssetPathLayout = new QFormLayout(AssetPathGroup);
		m_content_asset_path_edit_ = new QLineEdit(this);
		m_content_asset_path_edit_->setText(QString("Meshes/Imported/%1").arg(InModelName));
		m_content_asset_path_edit_->setPlaceholderText(tr("例如 Meshes/Characters/Soldier"));
		AssetPathLayout->addRow(tr("路径"), m_content_asset_path_edit_);
		RootLayout->addWidget(AssetPathGroup);
		resize(420, 180);
	}
	else
	{
		auto* PositionGroup = new QGroupBox(tr("位置"), this);
		auto* PositionLayout = new QFormLayout(PositionGroup);
		m_position_x_spin_ = CreateSpinBox(-100000.0, 100000.0, 0.0);
		m_position_y_spin_ = CreateSpinBox(-100000.0, 100000.0, 0.0);
		m_position_z_spin_ = CreateSpinBox(-100000.0, 100000.0, 0.0);
		PositionLayout->addRow(tr("X"), m_position_x_spin_);
		PositionLayout->addRow(tr("Y"), m_position_y_spin_);
		PositionLayout->addRow(tr("Z"), m_position_z_spin_);
		RootLayout->addWidget(PositionGroup);

		auto* RotationGroup = new QGroupBox(tr("旋转（度）"), this);
		auto* RotationLayout = new QFormLayout(RotationGroup);
		m_rotation_pitch_spin_ = CreateSpinBox(-360.0, 360.0, 0.0);
		m_rotation_yaw_spin_ = CreateSpinBox(-360.0, 360.0, 0.0);
		m_rotation_roll_spin_ = CreateSpinBox(-360.0, 360.0, 0.0);
		RotationLayout->addRow(tr("Pitch"), m_rotation_pitch_spin_);
		RotationLayout->addRow(tr("Yaw"), m_rotation_yaw_spin_);
		RotationLayout->addRow(tr("Roll"), m_rotation_roll_spin_);
		RootLayout->addWidget(RotationGroup);

		auto* ScaleGroup = new QGroupBox(tr("缩放"), this);
		auto* ScaleLayout = new QFormLayout(ScaleGroup);
		m_scale_x_spin_ = CreateSpinBox(0.001, 1000.0, 1.0);
		m_scale_y_spin_ = CreateSpinBox(0.001, 1000.0, 1.0);
		m_scale_z_spin_ = CreateSpinBox(0.001, 1000.0, 1.0);
		ScaleLayout->addRow(tr("X"), m_scale_x_spin_);
		ScaleLayout->addRow(tr("Y"), m_scale_y_spin_);
		ScaleLayout->addRow(tr("Z"), m_scale_z_spin_);
		RootLayout->addWidget(ScaleGroup);
		resize(360, 420);
	}

	auto* ButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(ButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(ButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	RootLayout->addWidget(ButtonBox);
}

QString ImportModelTransformDialog::GetContentAssetPath() const
{
	if (m_content_asset_path_edit_ == nullptr)
	{
		return {};
	}
	return m_content_asset_path_edit_->text().trimmed();
}

QDoubleSpinBox* ImportModelTransformDialog::CreateSpinBox(double InMin, double InMax, double InValue, int InDecimals)
{
	auto* SpinBox = new QDoubleSpinBox(this);
	SpinBox->setRange(InMin, InMax);
	SpinBox->setDecimals(InDecimals);
	SpinBox->setValue(InValue);
	SpinBox->setSingleStep(0.1);
	return SpinBox;
}

FActorTransform ImportModelTransformDialog::GetActorTransform() const
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
