#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

#include <QObject>
#include <QString>

enum class EEditorLogSeverity : uint8_t
{
	Verbose,
	Info,
	Warning,
	Error,
};

struct FEditorLogEntry
{
	EEditorLogSeverity Severity = EEditorLogSeverity::Info;
	QString Category;
	QString Message;
	uint64_t SequenceId = 0;
};

class EditorOutputLog final : public QObject
{
	Q_OBJECT

public:
	static EditorOutputLog& Get();

	void Install();
	void Uninstall();

	void AppendLog(EEditorLogSeverity InSeverity, const QString& InCategory, const QString& InMessage);
	void AppendLog(EEditorLogSeverity InSeverity, const std::string& InCategory, const std::string& InMessage);
	std::deque<FEditorLogEntry> GetEntries() const;
	uint64_t GetNextSequenceId() const;

signals:
	void LogEntryAdded(const FEditorLogEntry& InEntry);

private:
	explicit EditorOutputLog(QObject* InParent = nullptr);

	void AppendLogInternal(FEditorLogEntry InEntry);
	void TrimEntriesIfNeeded();

	mutable std::mutex m_mutex_;
	std::deque<FEditorLogEntry> m_entries_;
	uint64_t m_next_sequence_id_ = 1;
	bool m_b_installed = false;
};
