#include "ui/DraggableDoubleSpinBox.h"

#include <algorithm>

#include <QEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOptionSpinBox>

namespace
{
constexpr int kDragStartThresholdPx = 3;
constexpr double kMinDragStep = 1.0;
} // namespace

DraggableDoubleSpinBox::DraggableDoubleSpinBox(QWidget* InParent)
	: QDoubleSpinBox(InParent)
{
	setKeyboardTracking(false);
	setFocusPolicy(Qt::StrongFocus);
	SetDragSensitivity(kMinDragStep);

	if (QLineEdit* LineEdit = lineEdit())
	{
		LineEdit->installEventFilter(this);
	}
}

void DraggableDoubleSpinBox::SetDragSensitivity(double InSensitivity)
{
	m_drag_sensitivity_ = std::max(InSensitivity, kMinDragStep);
}

bool DraggableDoubleSpinBox::IsPointOnStepButtons(const QPoint& InLocalPos) const
{
	QStyleOptionSpinBox StyleOption;
	initStyleOption(&StyleOption);

	const QRect UpButtonRect =
		style()->subControlRect(QStyle::CC_SpinBox, &StyleOption, QStyle::SC_SpinBoxUp, this);
	const QRect DownButtonRect =
		style()->subControlRect(QStyle::CC_SpinBox, &StyleOption, QStyle::SC_SpinBoxDown, this);
	return UpButtonRect.contains(InLocalPos) || DownButtonRect.contains(InLocalPos);
}

double DraggableDoubleSpinBox::ComputeDragStep(const QMouseEvent* InEvent) const
{
	double Step = (m_drag_sensitivity_ > 0.0) ? m_drag_sensitivity_ : kMinDragStep;
	Step = std::max(Step, kMinDragStep);
	if (InEvent->modifiers() & Qt::ControlModifier)
	{
		Step *= 10.0;
	}
	return Step;
}

void DraggableDoubleSpinBox::ApplyDragDelta(int InDeltaPx, const QMouseEvent* InEvent)
{
	const double Step = ComputeDragStep(InEvent);
	const double SnappedDelta = std::round(static_cast<double>(InDeltaPx) * Step);
	setValue(m_press_value_ + SnappedDelta);
}

bool DraggableDoubleSpinBox::ProcessMousePress(QMouseEvent* InEvent)
{
	if (!isEnabled() || InEvent->button() != Qt::LeftButton)
	{
		return false;
	}

	m_has_press_ = true;
	m_is_dragging_ = false;
	m_press_global_pos_ = InEvent->globalPosition().toPoint();
	m_press_value_ = value();
	InEvent->accept();
	return true;
}

bool DraggableDoubleSpinBox::ProcessMouseMove(QMouseEvent* InEvent)
{
	if (!m_has_press_ || !(InEvent->buttons() & Qt::LeftButton))
	{
		return false;
	}

	const int DeltaPx = InEvent->globalPosition().toPoint().x() - m_press_global_pos_.x();
	if (!m_is_dragging_ && std::abs(DeltaPx) >= kDragStartThresholdPx)
	{
		m_is_dragging_ = true;
		grabMouse();
		setCursor(Qt::SizeHorCursor);
		if (QLineEdit* LineEdit = lineEdit())
		{
			LineEdit->deselect();
		}
	}

	if (m_is_dragging_)
	{
		ApplyDragDelta(DeltaPx, InEvent);
		InEvent->accept();
		return true;
	}

	return false;
}

bool DraggableDoubleSpinBox::ProcessMouseRelease(QMouseEvent* InEvent)
{
	if (!m_has_press_ || InEvent->button() != Qt::LeftButton)
	{
		return false;
	}

	if (m_is_dragging_)
	{
		releaseMouse();
		unsetCursor();
	}
	else
	{
		setFocus(Qt::MouseFocusReason);
		if (QLineEdit* LineEdit = lineEdit())
		{
			LineEdit->selectAll();
		}
	}

	m_has_press_ = false;
	m_is_dragging_ = false;
	InEvent->accept();
	return true;
}

bool DraggableDoubleSpinBox::eventFilter(QObject* InWatched, QEvent* InEvent)
{
	if (InWatched != lineEdit())
	{
		return QDoubleSpinBox::eventFilter(InWatched, InEvent);
	}

	switch (InEvent->type())
	{
	case QEvent::MouseButtonPress:
		return ProcessMousePress(static_cast<QMouseEvent*>(InEvent));
	case QEvent::MouseMove:
		return ProcessMouseMove(static_cast<QMouseEvent*>(InEvent));
	case QEvent::MouseButtonRelease:
		return ProcessMouseRelease(static_cast<QMouseEvent*>(InEvent));
	default:
		break;
	}

	return QDoubleSpinBox::eventFilter(InWatched, InEvent);
}

void DraggableDoubleSpinBox::mousePressEvent(QMouseEvent* InEvent)
{
	if (IsPointOnStepButtons(InEvent->pos()))
	{
		QDoubleSpinBox::mousePressEvent(InEvent);
		return;
	}

	if (ProcessMousePress(InEvent))
	{
		return;
	}

	QDoubleSpinBox::mousePressEvent(InEvent);
}

void DraggableDoubleSpinBox::mouseMoveEvent(QMouseEvent* InEvent)
{
	if (ProcessMouseMove(InEvent))
	{
		return;
	}

	QDoubleSpinBox::mouseMoveEvent(InEvent);
}

void DraggableDoubleSpinBox::mouseReleaseEvent(QMouseEvent* InEvent)
{
	if (ProcessMouseRelease(InEvent))
	{
		return;
	}

	QDoubleSpinBox::mouseReleaseEvent(InEvent);
}

void DraggableDoubleSpinBox::enterEvent(QEnterEvent* InEvent)
{
	if (!m_is_dragging_ && isEnabled())
	{
		setCursor(Qt::SizeHorCursor);
	}
	QDoubleSpinBox::enterEvent(InEvent);
}

void DraggableDoubleSpinBox::leaveEvent(QEvent* InEvent)
{
	if (!m_is_dragging_)
	{
		unsetCursor();
	}
	QDoubleSpinBox::leaveEvent(InEvent);
}
