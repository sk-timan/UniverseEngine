#include "ui/OutputLogPanelWidget.h"

#include <QFont>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

#include "ui/EditorOutputLog.h"

namespace
{
bool IsNearBottom(QTextEdit* InLogView)
{
	if (InLogView == nullptr || InLogView->verticalScrollBar() == nullptr)
	{
		return true;
	}

	QScrollBar* ScrollBar = InLogView->verticalScrollBar();
	return ScrollBar->maximum() - ScrollBar->value() <= 4;
}
}

OutputLogPanelWidget::OutputLogPanelWidget(QWidget* InParent)
	: QWidget(InParent)
{
	BuildUi();
	RefreshDisplay();

	connect(
		&EditorOutputLog::Get(),
		&EditorOutputLog::LogEntryAdded,
		this,
		&OutputLogPanelWidget::OnLogEntryAdded,
		Qt::QueuedConnection);
}

void OutputLogPanelWidget::BuildUi()
{
	auto* RootLayout = new QVBoxLayout(this);
	RootLayout->setContentsMargins(6, 6, 6, 6);
	RootLayout->setSpacing(6);

	auto* ToolbarLayout = new QHBoxLayout();
	m_search_edit_ = new QLineEdit(this);
	m_search_edit_->setObjectName("OutputLogSearchEdit");
	m_search_edit_->setPlaceholderText(tr("Search Log"));
	m_search_edit_->setClearButtonEnabled(true);
	ToolbarLayout->addWidget(m_search_edit_, 1);
	RootLayout->addLayout(ToolbarLayout);

	m_log_view_ = new QTextEdit(this);
	m_log_view_->setObjectName("OutputLogView");
	m_log_view_->setReadOnly(true);
	m_log_view_->setLineWrapMode(QTextEdit::NoWrap);
	m_log_view_->setUndoRedoEnabled(false);
	m_log_view_->document()->setMaximumBlockCount(10000);
	QFont LogFont = m_log_view_->font();
	LogFont.setFamily(QStringLiteral("Consolas"));
	LogFont.setStyleHint(QFont::Monospace);
	m_log_view_->setFont(LogFont);
	m_log_view_->setContextMenuPolicy(Qt::CustomContextMenu);
	RootLayout->addWidget(m_log_view_, 1);

	connect(m_search_edit_, &QLineEdit::textChanged, this, &OutputLogPanelWidget::OnSearchTextChanged);
	connect(
		m_log_view_,
		&QTextEdit::customContextMenuRequested,
		this,
		&OutputLogPanelWidget::OnLogContextMenuRequested);
	connect(
		m_log_view_->verticalScrollBar(),
		&QScrollBar::valueChanged,
		this,
		[this](int InValue)
		{
			(void)InValue;
			m_b_auto_scroll_to_bottom_ = IsNearBottom(m_log_view_);
		});
}

void OutputLogPanelWidget::RefreshDisplay()
{
	if (m_log_view_ == nullptr)
	{
		return;
	}

	const bool bShouldAutoScroll = IsNearBottom(m_log_view_);
	m_log_view_->clear();

	for (const FEditorLogEntry& Entry : EditorOutputLog::Get().GetEntries())
	{
		if (ShouldDisplayEntry(Entry))
		{
			AppendEntryToView(Entry);
		}
	}

	if (bShouldAutoScroll)
	{
		ScrollToBottomIfNeeded();
	}
}

void OutputLogPanelWidget::OnLogEntryAdded(const FEditorLogEntry& InEntry)
{
	if (!ShouldDisplayEntry(InEntry))
	{
		return;
	}

	AppendEntryToView(InEntry);
	ScrollToBottomIfNeeded();
}

void OutputLogPanelWidget::OnSearchTextChanged(const QString& InText)
{
	m_search_text_ = InText;
	RefreshDisplay();
}

void OutputLogPanelWidget::OnLogContextMenuRequested(const QPoint& InPos)
{
	if (m_log_view_ == nullptr)
	{
		return;
	}

	QMenu Menu(this);
	QAction* CopyAction = Menu.addAction(tr("复制"));
	CopyAction->setShortcut(QKeySequence::Copy);
	CopyAction->setEnabled(m_log_view_->textCursor().hasSelection());

	QAction* SelectAllAction = Menu.addAction(tr("全选"));
	SelectAllAction->setShortcut(QKeySequence::SelectAll);

	Menu.addSeparator();

	QAction* ClearAction = Menu.addAction(tr("清除"));

	QAction* Chosen = Menu.exec(m_log_view_->mapToGlobal(InPos));
	if (Chosen == CopyAction)
	{
		m_log_view_->copy();
	}
	else if (Chosen == SelectAllAction)
	{
		m_log_view_->selectAll();
	}
	else if (Chosen == ClearAction)
	{
		ClearDisplayedLogs();
	}
}

void OutputLogPanelWidget::ClearDisplayedLogs()
{
	if (m_log_view_ == nullptr)
	{
		return;
	}

	m_min_visible_sequence_id_ = EditorOutputLog::Get().GetNextSequenceId();
	m_log_view_->clear();
	m_b_auto_scroll_to_bottom_ = true;
}

bool OutputLogPanelWidget::IsEntryInVisibleRange(const FEditorLogEntry& InEntry) const
{
	return InEntry.SequenceId >= m_min_visible_sequence_id_;
}

bool OutputLogPanelWidget::ShouldDisplayEntry(const FEditorLogEntry& InEntry) const
{
	return IsEntryInVisibleRange(InEntry) && EntryMatchesFilter(InEntry);
}

bool OutputLogPanelWidget::EntryMatchesFilter(const FEditorLogEntry& InEntry) const
{
	const QString FilterText = m_search_text_.trimmed();
	if (FilterText.isEmpty())
	{
		return true;
	}

	const QString FilterLower = FilterText.toLower();
	return FormatLogLine(InEntry).toLower().contains(FilterLower);
}

QString OutputLogPanelWidget::FormatLogLine(const FEditorLogEntry& InEntry) const
{
	return QStringLiteral("%1: %2").arg(InEntry.Category, InEntry.Message);
}

QColor OutputLogPanelWidget::ColorForSeverity(EEditorLogSeverity InSeverity) const
{
	switch (InSeverity)
	{
	case EEditorLogSeverity::Verbose:
		return QColor("#808080");
	case EEditorLogSeverity::Warning:
		return QColor("#cca700");
	case EEditorLogSeverity::Error:
		return QColor("#f14c4c");
	case EEditorLogSeverity::Info:
	default:
		return QColor("#d4d4d4");
	}
}

void OutputLogPanelWidget::AppendEntryToView(const FEditorLogEntry& InEntry)
{
	if (m_log_view_ == nullptr)
	{
		return;
	}

	QTextCursor Cursor = m_log_view_->textCursor();
	Cursor.movePosition(QTextCursor::End);

	QTextCharFormat LineFormat;
	LineFormat.setForeground(ColorForSeverity(InEntry.Severity));
	Cursor.setCharFormat(LineFormat);
	Cursor.insertText(FormatLogLine(InEntry) + QLatin1Char('\n'));
	m_log_view_->setTextCursor(Cursor);
}

void OutputLogPanelWidget::ScrollToBottomIfNeeded()
{
	if (m_log_view_ == nullptr || !m_b_auto_scroll_to_bottom_)
	{
		return;
	}

	QScrollBar* ScrollBar = m_log_view_->verticalScrollBar();
	if (ScrollBar != nullptr)
	{
		ScrollBar->setValue(ScrollBar->maximum());
	}
}
