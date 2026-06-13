#include "ClangAstScanner.h"

#include "MacroParser.h"

#include <clang-c/CXCompilationDatabase.h>
#include <clang-c/CXSourceLocation.h>
#include <clang-c/CXString.h>
#include <clang-c/Index.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
std::string ToStdString(CXString InString)
{
	const char* Data = clang_getCString(InString);
	std::string Result = Data != nullptr ? Data : "";
	clang_disposeString(InString);
	return Result;
}

bool IsSameFilePath(const std::string& InRawPath, const std::filesystem::path& InTargetPath)
{
	if (InRawPath.empty())
	{
		return false;
	}

	std::error_code Ec;
	std::filesystem::path CursorPath = std::filesystem::weakly_canonical(std::filesystem::path(InRawPath), Ec);
	if (Ec)
	{
		CursorPath = std::filesystem::path(InRawPath);
	}

	std::filesystem::path TargetPath = std::filesystem::weakly_canonical(InTargetPath, Ec);
	if (Ec)
	{
		TargetPath = InTargetPath;
	}

	std::string CursorString = CursorPath.generic_string();
	std::string TargetString = TargetPath.generic_string();
	std::transform(CursorString.begin(), CursorString.end(), CursorString.begin(),
		[](unsigned char InChar) { return static_cast<char>(std::tolower(InChar)); });
	std::transform(TargetString.begin(), TargetString.end(), TargetString.begin(),
		[](unsigned char InChar) { return static_cast<char>(std::tolower(InChar)); });
	return CursorString == TargetString;
}

std::filesystem::path GetCompilationDatabaseDirectory(const std::filesystem::path& InCompileCommandsPath)
{
	if (InCompileCommandsPath.has_filename() && InCompileCommandsPath.filename() == "compile_commands.json")
	{
		return InCompileCommandsPath.parent_path();
	}

	return InCompileCommandsPath;
}

struct FAstScanContext
{
	std::vector<FReflectionClassRecord>* ClassRecords = nullptr;
	std::vector<FReflectionMacroRecord> MacroRecords;
	std::filesystem::path HeaderPath;
};

bool IsRecordCursor(CXCursorKind InKind)
{
	return InKind == CXCursor_ClassDecl || InKind == CXCursor_StructDecl;
}

CXChildVisitResult VisitRecordMember(CXCursor InCursor, CXCursor InParent, CXClientData InClientData)
{
	(void)InParent;
	FReflectionClassRecord* MutableClassRecord = static_cast<FReflectionClassRecord*>(InClientData);
	if (MutableClassRecord == nullptr)
	{
		return CXChildVisit_Continue;
	}

	const CXCursorKind Kind = clang_getCursorKind(InCursor);
	if (Kind == CXCursor_FieldDecl)
	{
		CXSourceLocation FieldLocation = clang_getCursorLocation(InCursor);
		unsigned FieldLine = 0;
		clang_getSpellingLocation(FieldLocation, nullptr, &FieldLine, nullptr, nullptr);

		FReflectionFieldRecord FieldRecord{};
		FieldRecord.Name = ToStdString(clang_getCursorSpelling(InCursor));
		FieldRecord.TypeSpelling = ToStdString(clang_getTypeSpelling(clang_getCursorType(InCursor)));
		FieldRecord.Line = static_cast<int32_t>(FieldLine);

		const CXType RecordType = clang_getCursorType(InParent);
		const int64_t FieldOffsetBits = clang_Type_getOffsetOf(RecordType, FieldRecord.Name.c_str());
		if (FieldOffsetBits >= 0)
		{
			FieldRecord.OffsetBytes = static_cast<std::size_t>(FieldOffsetBits) / 8;
		}

		MutableClassRecord->Fields.push_back(std::move(FieldRecord));
		return CXChildVisit_Continue;
	}

	if (Kind == CXCursor_CXXMethod || Kind == CXCursor_FunctionDecl)
	{
		if (clang_CXXMethod_isStatic(InCursor) != 0)
		{
			return CXChildVisit_Continue;
		}

		CXSourceLocation MethodLocation = clang_getCursorLocation(InCursor);
		unsigned MethodLine = 0;
		clang_getSpellingLocation(MethodLocation, nullptr, &MethodLine, nullptr, nullptr);

		FReflectionMethodRecord MethodRecord{};
		MethodRecord.Name = ToStdString(clang_getCursorSpelling(InCursor));
		MethodRecord.ReturnTypeSpelling = ToStdString(clang_getTypeSpelling(clang_getCursorResultType(InCursor)));
		MethodRecord.Line = static_cast<int32_t>(MethodLine);
		MutableClassRecord->Methods.push_back(std::move(MethodRecord));
	}

	if (Kind == CXCursor_CXXBaseSpecifier && MutableClassRecord->SuperClassName.empty())
	{
		MutableClassRecord->SuperClassName = ToStdString(clang_getTypeSpelling(clang_getCursorType(InCursor)));
	}

	return CXChildVisit_Continue;
}

CXChildVisitResult VisitTranslationUnitCursor(CXCursor InCursor, CXCursor InParent, CXClientData InClientData)
{
	(void)InParent;
	FAstScanContext* Context = static_cast<FAstScanContext*>(InClientData);
	if (Context == nullptr || Context->ClassRecords == nullptr)
	{
		return CXChildVisit_Continue;
	}

	if (!IsRecordCursor(clang_getCursorKind(InCursor)))
	{
		return CXChildVisit_Recurse;
	}

	if (clang_Cursor_isAnonymous(InCursor) || clang_Cursor_isAnonymousRecordDecl(InCursor))
	{
		return CXChildVisit_Continue;
	}

	CXSourceLocation Location = clang_getCursorLocation(InCursor);
	CXFile CursorFile{};
	unsigned Line = 0;
	clang_getSpellingLocation(Location, &CursorFile, &Line, nullptr, nullptr);

	const std::string CursorFileName = ToStdString(clang_getFileName(CursorFile));
	if (!IsSameFilePath(CursorFileName, Context->HeaderPath))
	{
		return CXChildVisit_Continue;
	}

	if (Line == 0)
	{
		return CXChildVisit_Continue;
	}

	if (clang_isCursorDefinition(InCursor) == 0)
	{
		return CXChildVisit_Continue;
	}

	const std::string ClassName = ToStdString(clang_getCursorSpelling(InCursor));
	if (ClassName.empty())
	{
		return CXChildVisit_Continue;
	}

	const FReflectionMacroRecord* ClassMacro =
		FindMacroOnOrAboveLine(Context->MacroRecords, static_cast<int32_t>(Line), EReflectionMacroType::UClass);
	bool bIsStruct = false;
	if (ClassMacro == nullptr)
	{
		ClassMacro = FindMacroOnOrAboveLine(Context->MacroRecords, static_cast<int32_t>(Line), EReflectionMacroType::UStruct);
		bIsStruct = ClassMacro != nullptr;
	}

	if (ClassMacro == nullptr)
	{
		return CXChildVisit_Continue;
	}

	FReflectionClassRecord ClassRecord{};
	ClassRecord.ClassName = ClassName;
	ClassRecord.HeaderPath = Context->HeaderPath;
	ClassRecord.ClassLine = static_cast<int32_t>(Line);
	ClassRecord.bIsStruct = bIsStruct;
	clang_visitChildren(InCursor, VisitRecordMember, &ClassRecord);
	Context->ClassRecords->push_back(std::move(ClassRecord));
	return CXChildVisit_Continue;
}

bool ShouldSkipRawCompileArgument(const std::string& InArg)
{
	if (InArg.find("cl.exe") != std::string::npos)
	{
		return true;
	}

	static const char* kSkippedArgs[] = {
		"/nologo", "-nologo",
		"/TP", "-TP",
		"/EHsc", "-EHsc",
		"/FS", "-FS",
		"/RTC1", "-RTC1",
		"/MDd", "-MDd",
		"/MD", "-MD",
		"/MTd", "-MTd",
		"/MT", "-MT",
		"/ZI", "-ZI",
		"/Zi", "-Zi",
		"/Z7", "-Z7",
		"/utf-8", "-utf-8",
		"/Ob0", "-Ob0",
		"/Ob1", "-Ob1",
		"/Ob2", "-Ob2",
		"/Od", "-Od",
		"/O1", "-O1",
		"/O2", "-O2",
		"-c",
		"/permissive-", "-permissive-",
		"/Zc:__cplusplus", "-Zc:__cplusplus",
		"/showIncludes", "-showIncludes",
	};

	for (const char* SkippedArg : kSkippedArgs)
	{
		if (InArg == SkippedArg)
		{
			return true;
		}
	}

	if (InArg.rfind("/Fo", 0) == 0 || InArg.rfind("/Fd", 0) == 0 ||
	    InArg.rfind("-Fo", 0) == 0 || InArg.rfind("-Fd", 0) == 0)
	{
		return true;
	}

	if (InArg.rfind("-external:W", 0) == 0)
	{
		return true;
	}

	if (InArg.rfind("--driver-mode=", 0) == 0)
	{
		return true;
	}

	return false;
}

bool TryNormalizeCompileArgument(const std::string& InArg, std::string* OutNormalizedArg)
{
	if (OutNormalizedArg == nullptr)
	{
		return false;
	}

	if (ShouldSkipRawCompileArgument(InArg))
	{
		return false;
	}

	if (InArg == "/Fo" || InArg == "/Fd" || InArg == "-Fo")
	{
		return false;
	}

	if (InArg.rfind("-external:I", 0) == 0)
	{
		*OutNormalizedArg = "-I" + InArg.substr(std::strlen("-external:I"));
		return true;
	}

	if (InArg.rfind("/I", 0) == 0 && InArg.size() > 2)
	{
		*OutNormalizedArg = "-I" + InArg.substr(2);
		return true;
	}

	if (InArg.rfind("/D", 0) == 0 && InArg.size() > 2)
	{
		*OutNormalizedArg = "-D" + InArg.substr(2);
		return true;
	}

	if (InArg.rfind("-std:", 0) == 0)
	{
		*OutNormalizedArg = "-std=" + InArg.substr(std::strlen("-std:"));
		return true;
	}

	if (InArg.rfind("/", 0) == 0)
	{
		return false;
	}

	*OutNormalizedArg = InArg;
	return true;
}

bool IsSourceFileArgument(const std::string& InArg)
{
	if (InArg.size() >= 4)
	{
		const std::string Extension = InArg.substr(InArg.size() - 4);
		if (Extension == ".cpp" || Extension == ".cxx")
		{
			return true;
		}
	}

	return InArg.size() >= 3 && InArg.substr(InArg.size() - 3) == ".cc";
}

bool IsHeaderFileArgument(const std::string& InArg)
{
	if (InArg.size() >= 4 && InArg.substr(InArg.size() - 4) == ".hpp")
	{
		return true;
	}

	return InArg.size() >= 2 && InArg.substr(InArg.size() - 2) == ".h";
}

std::vector<std::string> SanitizeCompileArgumentsForLibClang(const std::vector<std::string>& InRawArguments)
{
	std::vector<std::string> SanitizedArguments;
	SanitizedArguments.push_back("-target");
	SanitizedArguments.push_back("x86_64-pc-windows-msvc");
	SanitizedArguments.push_back("-fms-compatibility");
	SanitizedArguments.push_back("-fms-extensions");

	bool bHasStd = false;
	for (const std::string& RawArg : InRawArguments)
	{
		// libclang's cl driver mode injects a "--" separator followed by the input
		// file(s); everything after it would be treated as a source path, so stop.
		if (RawArg == "--")
		{
			break;
		}

		if (IsSourceFileArgument(RawArg) || IsHeaderFileArgument(RawArg))
		{
			continue;
		}

		std::string NormalizedArg;
		if (!TryNormalizeCompileArgument(RawArg, &NormalizedArg))
		{
			continue;
		}

		if (NormalizedArg.rfind("-std=", 0) == 0)
		{
			bHasStd = true;
		}

		SanitizedArguments.push_back(std::move(NormalizedArg));
	}

	if (!bHasStd)
	{
		SanitizedArguments.push_back("-std=c++20");
	}

	return SanitizedArguments;
}

std::vector<std::string> BuildCompileArguments(CXCompileCommands InCommands, const std::filesystem::path& InHeaderPath)
{
	std::vector<std::string> RawArguments;
	const unsigned CommandCount = clang_CompileCommands_getSize(InCommands);
	if (CommandCount == 0)
	{
		return {};
	}

	CXCompileCommand Command = clang_CompileCommands_getCommand(InCommands, 0);
	const unsigned ArgCount = clang_CompileCommand_getNumArgs(Command);
	for (unsigned ArgIndex = 0; ArgIndex < ArgCount; ++ArgIndex)
	{
		const std::string Arg = ToStdString(clang_CompileCommand_getArg(Command, ArgIndex));
		if (Arg == "-o" || Arg == "/Fo")
		{
			++ArgIndex;
			continue;
		}

		RawArguments.push_back(Arg);
	}

	(void)InHeaderPath;
	std::vector<std::string> Arguments = SanitizeCompileArgumentsForLibClang(RawArguments);
	Arguments.push_back("-x");
	Arguments.push_back("c++");
	return Arguments;
}

std::vector<const char*> ToArgv(const std::vector<std::string>& InArguments)
{
	std::vector<const char*> Argv;
	Argv.reserve(InArguments.size());
	for (const std::string& Arg : InArguments)
	{
		Argv.push_back(Arg.c_str());
	}
	return Argv;
}
} // namespace

FClangAstScanner::FClangAstScanner(std::filesystem::path InCompileCommandsPath)
	: CompileCommandsPath_(std::move(InCompileCommandsPath))
{
	if (!std::filesystem::exists(CompileCommandsPath_))
	{
		LastError_ = "compile_commands.json not found: " + CompileCommandsPath_.string();
	}
}

bool FClangAstScanner::IsReady() const
{
	return LastError_.empty();
}

const std::string& FClangAstScanner::GetLastError() const
{
	return LastError_;
}

std::vector<FReflectionClassRecord> FClangAstScanner::ScanHeader(
	const std::filesystem::path& InHeaderPath,
	const std::vector<FReflectionMacroRecord>& InMacroRecords)
{
	std::vector<FReflectionClassRecord> ClassRecords;
	if (!IsReady())
	{
		return ClassRecords;
	}

	CXCompilationDatabase_Error DbError = CXCompilationDatabase_NoError;
	const std::filesystem::path DbDirectory = GetCompilationDatabaseDirectory(CompileCommandsPath_);
	CXCompilationDatabase Database = clang_CompilationDatabase_fromDirectory(DbDirectory.string().c_str(), &DbError);
	if (Database == nullptr)
	{
		LastError_ = "Failed to open compilation database at " + DbDirectory.string();
		return ClassRecords;
	}

	CXCompileCommands Commands = clang_CompilationDatabase_getCompileCommands(Database, InHeaderPath.string().c_str());
	bool bUsedCppFallback = false;
	if (Commands == nullptr)
	{
		std::error_code Ec;
		for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(InHeaderPath.parent_path(), Ec))
		{
			if (!Entry.is_regular_file())
			{
				continue;
			}

			const std::string Extension = Entry.path().extension().string();
			if (Extension != ".cpp" && Extension != ".cc" && Extension != ".cxx")
			{
				continue;
			}

			Commands = clang_CompilationDatabase_getCompileCommands(Database, Entry.path().string().c_str());
			if (Commands != nullptr)
			{
				bUsedCppFallback = true;
				break;
			}
		}
	}

	(void)bUsedCppFallback;

	if (Commands == nullptr)
	{
		clang_CompilationDatabase_dispose(Database);
		LastError_ = "No compile command found for header: " + InHeaderPath.string();
		return ClassRecords;
	}

	const std::vector<std::string> CompileArguments = BuildCompileArguments(Commands, InHeaderPath);
	clang_CompileCommands_dispose(Commands);
	if (CompileArguments.empty())
	{
		clang_CompilationDatabase_dispose(Database);
		LastError_ = "Failed to build compile arguments for header: " + InHeaderPath.string();
		return ClassRecords;
	}

	// Parse the header through a synthetic .cpp translation unit. Passing a ".h"
	// file directly makes libclang treat it as a header to precompile, which fails
	// with CXError_ASTReadError; wrapping it in a .cpp that includes the header
	// keeps a normal TU parse.
	const std::filesystem::path WrapperPath = InHeaderPath.parent_path() / "__ost_reflection_tu.cpp";
	const std::string WrapperSource = "#include \"" + InHeaderPath.generic_string() + "\"\n";
	const std::string WrapperPathString = WrapperPath.string();
	CXUnsavedFile UnsavedFile{};
	UnsavedFile.Filename = WrapperPathString.c_str();
	UnsavedFile.Contents = WrapperSource.c_str();
	UnsavedFile.Length = static_cast<unsigned long>(WrapperSource.size());

	const std::vector<const char*> Argv = ToArgv(CompileArguments);
	CXIndex Index = clang_createIndex(0, 0);
	CXTranslationUnit TranslationUnit = nullptr;
	const CXErrorCode ParseError = clang_parseTranslationUnit2(
		Index,
		WrapperPathString.c_str(),
		Argv.data(),
		static_cast<int>(Argv.size()),
		&UnsavedFile,
		1,
		CXTranslationUnit_KeepGoing,
		&TranslationUnit);

	if (TranslationUnit == nullptr)
	{
		clang_disposeIndex(Index);
		clang_CompilationDatabase_dispose(Database);
		LastError_ = "clang_parseTranslationUnit failed for header: " + InHeaderPath.string() +
		             " (CXErrorCode=" + std::to_string(static_cast<int>(ParseError)) + ")";
		return ClassRecords;
	}

	// Diagnostics are intentionally NOT treated as fatal: with KeepGoing, libclang
	// still produces a usable AST for record layout even when some includes fail.

	FAstScanContext Context{};
	Context.ClassRecords = &ClassRecords;
	Context.MacroRecords = InMacroRecords;
	Context.HeaderPath = InHeaderPath;

	CXCursor RootCursor = clang_getTranslationUnitCursor(TranslationUnit);
	clang_visitChildren(RootCursor, VisitTranslationUnitCursor, &Context);

	clang_disposeTranslationUnit(TranslationUnit);
	clang_disposeIndex(Index);
	clang_CompilationDatabase_dispose(Database);
	LastError_.clear();

	return ClassRecords;
}
