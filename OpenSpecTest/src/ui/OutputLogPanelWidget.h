#pragma once

#include <cstdint>

#include <QColor>
#include <QWidget>

#include "ui/EditorOutputLog.h"

class QLineEdit;
class QTextEdit;

class OutputLogPanelWidget final : public QWidget
{
	Q_OBJECT

public:
	explicit OutputLogPanelWidget(QWidget* InParent = nullptr);

private:
	void BuildUi();
	void RefreshDisplay();
	void OnLogEntryAdded(const FEditorLogEntry& InEntry);
	void OnSearchTextChanged(const QString& InText);
	void OnLogContextMenuRequested(const QPoint& InPos);
	void ClearDisplayedLogs();
	bool IsEntryInVisibleRange(const FEditorLogEntry& InEntry) const;
	bool ShouldDisplayEntry(const FEditorLogEntry& InEntry) const;
	bool EntryMatchesFilter(const FEditorLogEntry& InEntry) const;
	QString FormatLogLine(const FEditorLogEntry& InEntry) const;
	QColor ColorForSeverity(EEditorLogSeverity InSeverity) const;
	void AppendEntryToView(const FEditorLogEntry& InEntry);
	void ScrollToBottomIfNeeded();

	QLineEdit* m_search_edit_ = nullptr;
	QTextEdit* m_log_view_ = nullptr;
	QString m_search_text_;
	uint64_t m_min_visible_sequence_id_ = 1;
	bool m_b_auto_scroll_to_bottom_ = true;
};
