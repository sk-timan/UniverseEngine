#include "ui/ScrollablePanelWidget.h"

#include <QFrame>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

ScrollablePanelWidget::ScrollablePanelWidget(QWidget* InParent, QMargins InContentMargins)
	: QWidget(InParent)
{
	auto* RootLayout = new QVBoxLayout(this);
	RootLayout->setContentsMargins(0, 0, 0, 0);
	RootLayout->setSpacing(0);

	m_scroll_area_ = new QScrollArea(this);
	m_scroll_area_->setObjectName("EditorScrollArea");
	m_scroll_area_->setWidgetResizable(false);
	m_scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_scroll_area_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_scroll_area_->setFrameShape(QFrame::NoFrame);
	RootLayout->addWidget(m_scroll_area_);

	m_content_widget_ = new QWidget(m_scroll_area_);
	m_content_widget_->setObjectName("EditorScrollContent");
	m_content_widget_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

	m_content_layout_ = new QVBoxLayout(m_content_widget_);
	m_content_layout_->setContentsMargins(InContentMargins);
	m_content_layout_->setSpacing(6);

	m_scroll_area_->setWidget(m_content_widget_);
}

QWidget* ScrollablePanelWidget::GetContentWidget() const
{
	return m_content_widget_;
}

QVBoxLayout* ScrollablePanelWidget::GetContentLayout() const
{
	return m_content_layout_;
}

int ScrollablePanelWidget::GetContentViewportWidth() const
{
	if (m_scroll_area_ == nullptr)
	{
		return width();
	}
	return m_scroll_area_->viewport()->width();
}

void ScrollablePanelWidget::RefreshScrollContentGeometry()
{
	if (m_scroll_area_ == nullptr || m_content_widget_ == nullptr)
	{
		return;
	}

	m_content_widget_->adjustSize();
	const int ViewportWidth = std::max(0, m_scroll_area_->viewport()->width());
	m_content_widget_->setFixedWidth(ViewportWidth);
	m_content_widget_->setMinimumHeight(m_content_widget_->sizeHint().height());
}

void ScrollablePanelWidget::resizeEvent(QResizeEvent* InEvent)
{
	QWidget::resizeEvent(InEvent);
	RefreshScrollContentGeometry();
}
