#pragma once

#include "ui/ScrollablePanelWidget.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QVBoxLayout;
class QWidget;

class GameApp;

class DetailPanelWidget final : public ScrollablePanelWidget
{
	Q_OBJECT

public:
	explicit DetailPanelWidget(GameApp* InGameApp, QWidget* InParent = nullptr);

	void RefreshFromSelection();

protected:
	void resizeEvent(QResizeEvent* InEvent) override;

private:
	void BuildUi();
	void UpdateHeaderLabelLayoutWidth();
	void RebuildReflectionProperties();
	void OnSearchTextChanged(const QString& InText);
	void OnAabbDebugToggled(bool bIsChecked);
	void SyncAabbDebugCheckbox();
	void OnObbDebugToggled(bool bIsChecked);
	void SyncObbDebugCheckbox();
	void OnSectionBoundsDebugToggled(bool bIsChecked);
	void SyncSectionBoundsDebugCheckbox();
	void OnReflectionPropertyChanged();

	GameApp* m_game_app_ = nullptr;
	QLabel* m_header_label_ = nullptr;
	QLineEdit* m_search_edit_ = nullptr;
	QWidget* m_reflection_properties_host_ = nullptr;
	QVBoxLayout* m_reflection_properties_layout_ = nullptr;
	QCheckBox* m_aabb_debug_checkbox_ = nullptr;
	QCheckBox* m_obb_debug_checkbox_ = nullptr;
	QCheckBox* m_section_bounds_debug_checkbox_ = nullptr;
	uint64_t m_last_selected_actor_id_ = 0;
	uint32_t m_last_scene_revision_ = 0;
};
