#pragma once

#include <QDialog>

#include "editor/EditorPerformanceSettings.h"

class QButtonGroup;
class QDialogButtonBox;
class QRadioButton;

class EditorPerformanceDialog final : public QDialog
{
	Q_OBJECT

public:
	explicit EditorPerformanceDialog(const FEditorPerformanceSettings& InInitialSettings, QWidget* InParent = nullptr);

	FEditorPerformanceSettings GetSettings() const;

private:
	void BuildUi();

	QRadioButton* m_median_split_radio_ = nullptr;
	QRadioButton* m_sah_split_radio_ = nullptr;
	QButtonGroup* m_split_method_group_ = nullptr;
};
