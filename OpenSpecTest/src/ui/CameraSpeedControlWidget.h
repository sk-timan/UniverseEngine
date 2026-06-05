#pragma once

#include <QWidget>

class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;

class CameraSpeedControlWidget final : public QWidget
{
	Q_OBJECT

public:
	explicit CameraSpeedControlWidget(QWidget* InParent = nullptr);

	void SetCameraSpeed(int InSpeed);
	void SetCameraSpeedScalar(float InScalar);
	int GetCameraSpeed() const;
	float GetCameraSpeedScalar() const;

signals:
	void CameraSpeedChanged(int InSpeed);
	void CameraSpeedScalarChanged(float InScalar);

protected:
	bool eventFilter(QObject* InWatched, QEvent* InEvent) override;

private slots:
	void OnTriggerClicked();
	void OnSpeedSliderChanged(int InValue);
	void OnScalarEditingFinished();

private:
	void BuildUi();
	void UpdateTriggerLabel();
	void UpdateSpeedValueLabel();
	void PositionPopup();
	void SetPopupVisible(bool bIsVisible);

	QPushButton* m_trigger_button_ = nullptr;
	QFrame* m_popup_frame_ = nullptr;
	QLabel* m_speed_value_label_ = nullptr;
	QSlider* m_speed_slider_ = nullptr;
	QLineEdit* m_scalar_edit_ = nullptr;
	int m_camera_speed_ = 8;
	float m_camera_speed_scalar_ = 1.0f;
	bool m_is_popup_visible_ = false;
	bool m_is_syncing_controls_ = false;
};
