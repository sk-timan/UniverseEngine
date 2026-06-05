#pragma once

#include <QDoubleSpinBox>
#include <QEnterEvent>
#include <QMouseEvent>

class DraggableDoubleSpinBox final : public QDoubleSpinBox
{
	Q_OBJECT

public:
	explicit DraggableDoubleSpinBox(QWidget* InParent = nullptr);

	void SetDragSensitivity(double InSensitivity);

protected:
	bool eventFilter(QObject* InWatched, QEvent* InEvent) override;
	void mousePressEvent(QMouseEvent* InEvent) override;
	void mouseMoveEvent(QMouseEvent* InEvent) override;
	void mouseReleaseEvent(QMouseEvent* InEvent) override;
	void enterEvent(QEnterEvent* InEvent) override;
	void leaveEvent(QEvent* InEvent) override;

private:
	bool IsPointOnStepButtons(const QPoint& InLocalPos) const;
	bool ProcessMousePress(QMouseEvent* InEvent);
	bool ProcessMouseMove(QMouseEvent* InEvent);
	bool ProcessMouseRelease(QMouseEvent* InEvent);
	void ApplyDragDelta(int InDeltaPx, const QMouseEvent* InEvent);
	double ComputeDragStep(const QMouseEvent* InEvent) const;

	bool m_has_press_ = false;
	bool m_is_dragging_ = false;
	QPoint m_press_global_pos_{};
	double m_press_value_ = 0.0;
	double m_drag_sensitivity_ = 1.0;
};
