#include "ui/EditorOutputLog.h"

#include <iostream>
#include <streambuf>
#include <string>

#include <QMessageLogContext>
#include <QMetaType>

Q_DECLARE_METATYPE(FEditorLogEntry)

namespace
{
constexpr size_t kMaxLogEntryCount = 5000;

QtMessageHandler g_previous_qt_message_handler = nullptr;

EEditorLogSeverity SeverityFromQtMessageType(QtMsgType InType)
{
	switch (InType)
	{
	case QtDebugMsg:
		return EEditorLogSeverity::Verbose;
	case QtInfoMsg:
		return EEditorLogSeverity::Info;
	case QtWarningMsg:
		return EEditorLogSeverity::Warning;
	case QtCriticalMsg:
	case QtFatalMsg:
	default:
		return EEditorLogSeverity::Error;
	}
}

class FConsoleLogStreamBuf final : public std::streambuf
{
public:
	explicit FConsoleLogStreamBuf(EEditorLogSeverity InSeverity, const char* InCategory)
		: m_severity(InSeverity)
		, m_category(InCategory)
	{
	}

protected:
	int overflow(int InCharacter) override
	{
		if (InCharacter == EOF)
		{
			return InCharacter;
		}

		const char Character = static_cast<char>(InCharacter);
		m_pending_line.push_back(Character);
		if (Character == '\n')
		{
			FlushPendingLine();
		}

		return InCharacter;
	}

	int sync() override
	{
		FlushPendingLine();
		return 0;
	}

private:
	void FlushPendingLine()
	{
		if (m_pending_line.empty())
		{
			return;
		}

		QString Line = QString::fromLocal8Bit(m_pending_line.data(), static_cast<int>(m_pending_line.size()));
		Line = Line.trimmed();
		if (!Line.isEmpty())
		{
			EditorOutputLog::Get().AppendLog(m_severity, m_category, Line);
		}

		m_pending_line.clear();
	}

	EEditorLogSeverity m_severity = EEditorLogSeverity::Info;
	const char* m_category = "Console";
	std::string m_pending_line;
};

FConsoleLogStreamBuf g_stdout_log_stream_buf(EEditorLogSeverity::Info, "Console");
FConsoleLogStreamBuf g_stderr_log_stream_buf(EEditorLogSeverity::Error, "ConsoleError");
std::streambuf* g_previous_stdout_stream_buf = nullptr;
std::streambuf* g_previous_stderr_stream_buf = nullptr;

void EditorQtMessageHandler(QtMsgType InType, const QMessageLogContext& InContext, const QString& InMessage)
{
	if (g_previous_qt_message_handler != nullptr)
	{
		g_previous_qt_message_handler(InType, InContext, InMessage);
	}

	QString Category = InContext.category != nullptr ? QString::fromUtf8(InContext.category) : QStringLiteral("Qt");
	if (Category.isEmpty())
	{
		Category = QStringLiteral("Qt");
	}

	EditorOutputLog::Get().AppendLog(SeverityFromQtMessageType(InType), Category, InMessage);
}
}

EditorOutputLog& EditorOutputLog::Get()
{
	static EditorOutputLog Instance;
	return Instance;
}

EditorOutputLog::EditorOutputLog(QObject* InParent)
	: QObject(InParent)
{
}

void EditorOutputLog::Install()
{
	if (m_b_installed)
	{
		return;
	}

	static bool bMetaTypeRegistered = false;
	if (!bMetaTypeRegistered)
	{
		qRegisterMetaType<FEditorLogEntry>("FEditorLogEntry");
		bMetaTypeRegistered = true;
	}

	g_previous_qt_message_handler = qInstallMessageHandler(EditorQtMessageHandler);
	g_previous_stdout_stream_buf = std::cout.rdbuf(&g_stdout_log_stream_buf);
	g_previous_stderr_stream_buf = std::cerr.rdbuf(&g_stderr_log_stream_buf);
	m_b_installed = true;

	AppendLog(EEditorLogSeverity::Info, "Log", tr("Output Log initialized."));
}

void EditorOutputLog::Uninstall()
{
	if (!m_b_installed)
	{
		return;
	}

	if (g_previous_stdout_stream_buf != nullptr)
	{
		std::cout.rdbuf(g_previous_stdout_stream_buf);
		g_previous_stdout_stream_buf = nullptr;
	}
	if (g_previous_stderr_stream_buf != nullptr)
	{
		std::cerr.rdbuf(g_previous_stderr_stream_buf);
		g_previous_stderr_stream_buf = nullptr;
	}

	qInstallMessageHandler(g_previous_qt_message_handler);
	g_previous_qt_message_handler = nullptr;
	m_b_installed = false;
}

void EditorOutputLog::AppendLog(
	EEditorLogSeverity InSeverity,
	const QString& InCategory,
	const QString& InMessage)
{
	if (InMessage.isEmpty())
	{
		return;
	}

	FEditorLogEntry Entry;
	Entry.Severity = InSeverity;
	Entry.Category = InCategory.trimmed().isEmpty() ? QStringLiteral("Log") : InCategory.trimmed();
	Entry.Message = InMessage;
	AppendLogInternal(Entry);
}

void EditorOutputLog::AppendLog(
	EEditorLogSeverity InSeverity,
	const std::string& InCategory,
	const std::string& InMessage)
{
	AppendLog(
		InSeverity,
		QString::fromStdString(InCategory),
		QString::fromStdString(InMessage));
}

std::deque<FEditorLogEntry> EditorOutputLog::GetEntries() const
{
	std::lock_guard<std::mutex> Lock(m_mutex_);
	return m_entries_;
}

uint64_t EditorOutputLog::GetNextSequenceId() const
{
	std::lock_guard<std::mutex> Lock(m_mutex_);
	return m_next_sequence_id_;
}

void EditorOutputLog::AppendLogInternal(FEditorLogEntry InEntry)
{
	FEditorLogEntry StoredEntry = InEntry;
	{
		std::lock_guard<std::mutex> Lock(m_mutex_);
		StoredEntry.SequenceId = m_next_sequence_id_++;
		m_entries_.push_back(StoredEntry);
		TrimEntriesIfNeeded();
	}

	emit LogEntryAdded(StoredEntry);
}

void EditorOutputLog::TrimEntriesIfNeeded()
{
	while (m_entries_.size() > kMaxLogEntryCount)
	{
		m_entries_.pop_front();
	}
}
