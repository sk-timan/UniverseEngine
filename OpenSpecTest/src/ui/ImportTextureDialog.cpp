#include "ui/ImportTextureDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

ImportTextureDialog::ImportTextureDialog(const QString& InTextureName, QWidget* InParent)
	: QDialog(InParent)
{
	setWindowTitle(tr("导入纹理"));
	setModal(true);
	BuildUi(InTextureName);
}

void ImportTextureDialog::BuildUi(const QString& InTextureName)
{
	auto* RootLayout = new QVBoxLayout(this);

	auto* HintLabel = new QLabel(
		tr("即将导入源图片并写入 Content 资产: %1\n请指定 Content 路径与导入设置。").arg(InTextureName),
		this);
	HintLabel->setWordWrap(true);
	RootLayout->addWidget(HintLabel);

	auto* AssetPathGroup = new QGroupBox(tr("Content 资产路径"), this);
	auto* AssetPathLayout = new QFormLayout(AssetPathGroup);
	m_content_asset_path_edit_ = new QLineEdit(this);
	m_content_asset_path_edit_->setText(QString("Textures/Imported/%1").arg(InTextureName));
	m_content_asset_path_edit_->setPlaceholderText(tr("例如 Textures/Environment/Grass"));
	AssetPathLayout->addRow(tr("路径"), m_content_asset_path_edit_);
	RootLayout->addWidget(AssetPathGroup);

	auto* SettingsGroup = new QGroupBox(tr("导入设置"), this);
	auto* SettingsLayout = new QFormLayout(SettingsGroup);
	m_srgb_check_ = new QCheckBox(tr("sRGB"), this);
	m_srgb_check_->setChecked(true);
	m_flip_y_check_ = new QCheckBox(tr("Flip Y"), this);
	m_max_size_spin_ = new QSpinBox(this);
	m_max_size_spin_->setRange(1, 8192);
	m_max_size_spin_->setValue(4096);
	SettingsLayout->addRow(QString(), m_srgb_check_);
	SettingsLayout->addRow(QString(), m_flip_y_check_);
	SettingsLayout->addRow(tr("Max Size"), m_max_size_spin_);
	RootLayout->addWidget(SettingsGroup);

	auto* ButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(ButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(ButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	RootLayout->addWidget(ButtonBox);

	resize(420, 260);
}

QString ImportTextureDialog::GetContentAssetPath() const
{
	return m_content_asset_path_edit_ != nullptr ? m_content_asset_path_edit_->text().trimmed() : QString();
}

void ImportTextureDialog::SetContentAssetPath(const QString& InPath)
{
	if (m_content_asset_path_edit_ != nullptr)
	{
		m_content_asset_path_edit_->setText(InPath);
	}
}

FTextureImportSettings ImportTextureDialog::GetImportSettings() const
{
	FTextureImportSettings Settings;
	Settings.bSRGB = m_srgb_check_ != nullptr && m_srgb_check_->isChecked();
	Settings.bFlipY = m_flip_y_check_ != nullptr && m_flip_y_check_->isChecked();
	Settings.MaxSize = m_max_size_spin_ != nullptr ? m_max_size_spin_->value() : 4096;
	return Settings;
}
