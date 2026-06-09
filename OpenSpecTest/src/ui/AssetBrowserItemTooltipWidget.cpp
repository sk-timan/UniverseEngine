#include "ui/AssetBrowserItemTooltipWidget.h"

#include <QFont>
#include <QGuiApplication>
#include <QLabel>
#include <QScreen>
#include <QVBoxLayout>

namespace
{
constexpr int kTooltipMinWidth = 280;
constexpr int kTooltipMaxWidth = 420;
constexpr int kTooltipEdgeMargin = 8;
constexpr int kTooltipOffsetFromAnchor = 12;

QString BuildTitleText(const QString& InName, const QString& InTypeDisplayName)
{
	if (InTypeDisplayName.isEmpty())
	{
		return InName;
	}
	return InName + QStringLiteral(" (") + InTypeDisplayName + QStringLiteral(")");
}
} // namespace

AssetBrowserItemTooltipWidget::AssetBrowserItemTooltipWidget(QWidget* InParent)
	: QWidget(InParent, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
	setAttribute(Qt::WA_ShowWithoutActivating, true);
	setAttribute(Qt::WA_TranslucentBackground, false);
	BuildUi();
}

void AssetBrowserItemTooltipWidget::BuildUi()
{
	setObjectName("AssetBrowserItemTooltip");
	setStyleSheet(
		"#AssetBrowserItemTooltip {"
		"  background-color: #2b2b2b;"
		"  border: 1px solid #4a4a4a;"
		"  border-radius: 2px;"
		"}"
		"#AssetBrowserItemTooltipTitle {"
		"  color: #f2f2f2;"
		"  padding: 0px;"
		"}"
		"#AssetBrowserItemTooltipPath {"
		"  color: #b8b8b8;"
		"  padding: 0px;"
		"}");

	auto* RootLayout = new QVBoxLayout(this);
	RootLayout->setContentsMargins(10, 8, 10, 8);
	RootLayout->setSpacing(6);

	m_title_label_ = new QLabel(this);
	m_title_label_->setObjectName("AssetBrowserItemTooltipTitle");
	m_title_label_->setWordWrap(true);
	QFont TitleFont = m_title_label_->font();
	TitleFont.setPointSize(9);
	TitleFont.setBold(true);
	m_title_label_->setFont(TitleFont);
	RootLayout->addWidget(m_title_label_);

	m_path_label_ = new QLabel(this);
	m_path_label_->setObjectName("AssetBrowserItemTooltipPath");
	m_path_label_->setWordWrap(true);
	m_path_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
	QFont PathFont = m_path_label_->font();
	PathFont.setPointSize(8);
	m_path_label_->setFont(PathFont);
	RootLayout->addWidget(m_path_label_);

	setMinimumWidth(kTooltipMinWidth);
	setMaximumWidth(kTooltipMaxWidth);
}

void AssetBrowserItemTooltipWidget::SetContent(
	const QString& InName,
	const QString& InTypeDisplayName,
	const QString& InSoftPath)
{
	m_title_label_->setText(BuildTitleText(InName, InTypeDisplayName));
	m_path_label_->setText(tr("Path: %1").arg(InSoftPath));
	adjustSize();
}

void AssetBrowserItemTooltipWidget::ShowNearGlobalPos(
	const QPoint& InGlobalPos,
	const QRect& InAnchorScreenRect)
{
	(void)InGlobalPos;
	QScreen* Screen = screen();
	if (Screen == nullptr && parentWidget() != nullptr)
	{
		Screen = parentWidget()->screen();
	}
	if (Screen == nullptr)
	{
		Screen = QGuiApplication::primaryScreen();
	}

	const QRect ScreenGeometry = Screen != nullptr ? Screen->availableGeometry() : QRect();
	QPoint TooltipPos = InAnchorScreenRect.topRight() + QPoint(kTooltipOffsetFromAnchor, 0);
	if (TooltipPos.x() + width() > ScreenGeometry.right() - kTooltipEdgeMargin)
	{
		TooltipPos.setX(InAnchorScreenRect.left() - width() - kTooltipOffsetFromAnchor);
	}
	if (TooltipPos.y() + height() > ScreenGeometry.bottom() - kTooltipEdgeMargin)
	{
		TooltipPos.setY(ScreenGeometry.bottom() - height() - kTooltipEdgeMargin);
	}
	if (TooltipPos.x() < ScreenGeometry.left() + kTooltipEdgeMargin)
	{
		TooltipPos.setX(ScreenGeometry.left() + kTooltipEdgeMargin);
	}
	if (TooltipPos.y() < ScreenGeometry.top() + kTooltipEdgeMargin)
	{
		TooltipPos.setY(ScreenGeometry.top() + kTooltipEdgeMargin);
	}

	move(TooltipPos);
	show();
}
