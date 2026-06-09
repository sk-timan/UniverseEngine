#pragma once

#include <QWidget>

class QLabel;

class AssetBrowserItemTooltipWidget final : public QWidget
{
	Q_OBJECT

public:
	explicit AssetBrowserItemTooltipWidget(QWidget* InParent = nullptr);

	void SetContent(const QString& InName, const QString& InTypeDisplayName, const QString& InSoftPath);
	void ShowNearGlobalPos(const QPoint& InGlobalPos, const QRect& InAnchorScreenRect);

private:
	void BuildUi();

	QLabel* m_title_label_ = nullptr;
	QLabel* m_path_label_ = nullptr;
};
