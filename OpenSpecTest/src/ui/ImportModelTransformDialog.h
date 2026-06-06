#pragma once

#include <QDialog>

#include "world/ActorTransform.h"

class QDialogButtonBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;

enum class EImportModelDialogMode
{
	ImportAsset,
	LoadModel,
};

class ImportModelTransformDialog final : public QDialog
{
	Q_OBJECT

public:
	explicit ImportModelTransformDialog(
		const QString& InModelName,
		EImportModelDialogMode InMode,
		QWidget* InParent = nullptr);

	FActorTransform GetActorTransform() const;
	QString GetContentAssetPath() const;

private:
	void BuildUi(const QString& InModelName);
	QDoubleSpinBox* CreateSpinBox(double InMin, double InMax, double InValue, int InDecimals = 3);

	EImportModelDialogMode m_mode_ = EImportModelDialogMode::LoadModel;
	QDoubleSpinBox* m_position_x_spin_ = nullptr;
	QDoubleSpinBox* m_position_y_spin_ = nullptr;
	QDoubleSpinBox* m_position_z_spin_ = nullptr;
	QDoubleSpinBox* m_rotation_pitch_spin_ = nullptr;
	QDoubleSpinBox* m_rotation_yaw_spin_ = nullptr;
	QDoubleSpinBox* m_rotation_roll_spin_ = nullptr;
	QDoubleSpinBox* m_scale_x_spin_ = nullptr;
	QDoubleSpinBox* m_scale_y_spin_ = nullptr;
	QDoubleSpinBox* m_scale_z_spin_ = nullptr;
	QLineEdit* m_content_asset_path_edit_ = nullptr;
};
