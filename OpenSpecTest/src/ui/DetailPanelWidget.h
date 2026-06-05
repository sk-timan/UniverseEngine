#pragma once

#include <QResizeEvent>
#include <QWidget>

#include "world/ActorTransform.h"

class DraggableDoubleSpinBox;
class QCheckBox;
class QGroupBox;
class QLabel;
class QLineEdit;

class GameApp;

class DetailPanelWidget final : public QWidget
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
	DraggableDoubleSpinBox* CreateSpinBox(double InMin, double InMax, double InValue, int InDecimals = 3);
	void PopulateFromActorTransform(const FActorTransform& InTransform);
	FActorTransform BuildTransformFromControls() const;
	void CommitTransformFromControls();
	void OnTransformFieldChanged(double InValue);
	void OnTransformEditingFinished();
	void OnSearchTextChanged(const QString& InText);
	void OnAabbDebugToggled(bool bIsChecked);
	void SyncAabbDebugCheckbox();
	void OnObbDebugToggled(bool bIsChecked);
	void SyncObbDebugCheckbox();
	void OnSectionBoundsDebugToggled(bool bIsChecked);
	void SyncSectionBoundsDebugCheckbox();

	GameApp* m_game_app_ = nullptr;
	QLabel* m_header_label_ = nullptr;
	QLineEdit* m_search_edit_ = nullptr;
	QGroupBox* m_transform_group_ = nullptr;
	QCheckBox* m_aabb_debug_checkbox_ = nullptr;
	QCheckBox* m_obb_debug_checkbox_ = nullptr;
	QCheckBox* m_section_bounds_debug_checkbox_ = nullptr;
	DraggableDoubleSpinBox* m_position_x_spin_ = nullptr;
	DraggableDoubleSpinBox* m_position_y_spin_ = nullptr;
	DraggableDoubleSpinBox* m_position_z_spin_ = nullptr;
	DraggableDoubleSpinBox* m_rotation_pitch_spin_ = nullptr;
	DraggableDoubleSpinBox* m_rotation_yaw_spin_ = nullptr;
	DraggableDoubleSpinBox* m_rotation_roll_spin_ = nullptr;
	DraggableDoubleSpinBox* m_scale_x_spin_ = nullptr;
	DraggableDoubleSpinBox* m_scale_y_spin_ = nullptr;
	DraggableDoubleSpinBox* m_scale_z_spin_ = nullptr;
	uint64_t m_last_selected_actor_id_ = 0;
	uint32_t m_last_scene_revision_ = 0;
	bool m_is_syncing_controls_ = false;
};
