#pragma once

#include <QMargins>
#include <QWidget>

class QScrollArea;
class QVBoxLayout;

// 编辑器 Dock 面板基类：内容保持自然高度，空间不足时滚轮滚动，不挤压控件。
class ScrollablePanelWidget : public QWidget
{
	Q_OBJECT

public:
	explicit ScrollablePanelWidget(QWidget* InParent = nullptr, QMargins InContentMargins = QMargins(6, 6, 6, 6));

	QWidget* GetContentWidget() const;
	QVBoxLayout* GetContentLayout() const;
	int GetContentViewportWidth() const;

	void RefreshScrollContentGeometry();

protected:
	void resizeEvent(QResizeEvent* InEvent) override;

private:
	QScrollArea* m_scroll_area_ = nullptr;
	QWidget* m_content_widget_ = nullptr;
	QVBoxLayout* m_content_layout_ = nullptr;
};
