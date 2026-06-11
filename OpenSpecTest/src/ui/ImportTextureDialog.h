#pragma once

#include <QDialog>

#include "render/asset/TextureData.h"

class QCheckBox;
class QDialogButtonBox;
class QLineEdit;
class QSpinBox;

class ImportTextureDialog final : public QDialog
{
	Q_OBJECT

public:
	explicit ImportTextureDialog(const QString& InTextureName, QWidget* InParent = nullptr);

	QString GetContentAssetPath() const;
	void SetContentAssetPath(const QString& InPath);
	FTextureImportSettings GetImportSettings() const;

private:
	void BuildUi(const QString& InTextureName);

	QLineEdit* m_content_asset_path_edit_ = nullptr;
	QCheckBox* m_srgb_check_ = nullptr;
	QCheckBox* m_flip_y_check_ = nullptr;
	QSpinBox* m_max_size_spin_ = nullptr;
};
