#pragma once

#include <QDialog>

#include "data/EditorPreferences.h"

class QDialogButtonBox;
class QDoubleSpinBox;
class QSpinBox;

class EditorPreferencesDialog final : public QDialog
{
	Q_OBJECT

public:
	explicit EditorPreferencesDialog(const FEditorPreferences& InInitialPreferences, QWidget* InParent = nullptr);

	FEditorPreferences GetPreferences() const;

private:
	void BuildUi();

	QDoubleSpinBox* m_near_clip_spin_ = nullptr;
	QDoubleSpinBox* m_far_clip_spin_ = nullptr;
	QSpinBox* m_camera_speed_spin_ = nullptr;
	QDoubleSpinBox* m_camera_speed_scalar_spin_ = nullptr;
};
