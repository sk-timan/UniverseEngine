#include "ReflectionGeneratorApp.h"

#include "ClangAstScanner.h"
#include "CodeEmitter.h"
#include "MacroParser.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
void PrintUsage()
{
	std::cerr
		<< "Usage: ReflectionGenerator\n"
		<< "  --compile-commands <path>\n"
		<< "  --scan-dir <path>\n"
		<< "  --output-dir <path>\n"
		<< "  [--module <name>]\n";
}

bool FileContainsReflectionMarkers(const std::filesystem::path& InHeaderPath)
{
	std::ifstream Stream(InHeaderPath);
	if (!Stream.is_open())
	{
		return false;
	}

	std::string Line;
	bool bHasGeneratedBody = false;
	bool bHasClassMacro = false;
	while (std::getline(Stream, Line))
	{
		// Skip preprocessor lines so the macro-definition header itself
		// (which #defines UCLASS/GENERATED_BODY) is not treated as a reflection header.
		const std::size_t FirstNonSpace = Line.find_first_not_of(" \t");
		if (FirstNonSpace != std::string::npos && Line[FirstNonSpace] == '#')
		{
			continue;
		}

		if (Line.find("GENERATED_BODY()") != std::string::npos)
		{
			bHasGeneratedBody = true;
		}
		if (Line.find("UCLASS(") != std::string::npos || Line.find("USTRUCT(") != std::string::npos)
		{
			bHasClassMacro = true;
		}
	}

	return bHasGeneratedBody && bHasClassMacro;
}

std::vector<std::filesystem::path> CollectReflectionHeaders(const std::filesystem::path& InScanDirectory)
{
	std::vector<std::filesystem::path> Headers;
	if (!std::filesystem::exists(InScanDirectory))
	{
		return Headers;
	}

	for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(InScanDirectory))
	{
		if (!Entry.is_regular_file())
		{
			continue;
		}

		const std::filesystem::path& Path = Entry.path();
		if (Path.extension() != ".h" && Path.extension() != ".hpp")
		{
			continue;
		}

		if (FileContainsReflectionMarkers(Path))
		{
			Headers.push_back(Path);
		}
	}

	return Headers;
}
} // namespace

bool ParseReflectionGeneratorOptions(int InArgc, char* InArgv[], FReflectionGeneratorOptions* OutOptions, std::string* OutErrorMessage)
{
	if (OutOptions == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "OutOptions is null.";
		}
		return false;
	}

	for (int ArgIndex = 1; ArgIndex < InArgc; ++ArgIndex)
	{
		const std::string Arg = InArgv[ArgIndex];
		if (Arg == "--compile-commands")
		{
			if (ArgIndex + 1 >= InArgc)
			{
				if (OutErrorMessage != nullptr)
				{
					*OutErrorMessage = "Missing value for --compile-commands.";
				}
				PrintUsage();
				return false;
			}
			OutOptions->CompileCommandsPath = InArgv[++ArgIndex];
			continue;
		}

		if (Arg == "--scan-dir")
		{
			if (ArgIndex + 1 >= InArgc)
			{
				if (OutErrorMessage != nullptr)
				{
					*OutErrorMessage = "Missing value for --scan-dir.";
				}
				PrintUsage();
				return false;
			}
			OutOptions->ScanDirectory = InArgv[++ArgIndex];
			continue;
		}

		if (Arg == "--output-dir")
		{
			if (ArgIndex + 1 >= InArgc)
			{
				if (OutErrorMessage != nullptr)
				{
					*OutErrorMessage = "Missing value for --output-dir.";
				}
				PrintUsage();
				return false;
			}
			OutOptions->OutputDirectory = InArgv[++ArgIndex];
			continue;
		}

		if (Arg == "--module")
		{
			if (ArgIndex + 1 >= InArgc)
			{
				if (OutErrorMessage != nullptr)
				{
					*OutErrorMessage = "Missing value for --module.";
				}
				PrintUsage();
				return false;
			}
			OutOptions->ModuleName = InArgv[++ArgIndex];
			continue;
		}

		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Unknown argument: " + Arg;
		}
		PrintUsage();
		return false;
	}

	if (OutOptions->CompileCommandsPath.empty())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "--compile-commands is required.";
		}
		PrintUsage();
		return false;
	}

	if (OutOptions->ScanDirectory.empty())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "--scan-dir is required.";
		}
		PrintUsage();
		return false;
	}

	if (OutOptions->OutputDirectory.empty())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "--output-dir is required.";
		}
		PrintUsage();
		return false;
	}

	if (!std::filesystem::exists(OutOptions->CompileCommandsPath))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "compile_commands.json not found: " + OutOptions->CompileCommandsPath.string();
		}
		return false;
	}

	return true;
}

int RunReflectionGenerator(const FReflectionGeneratorOptions& InOptions)
{
	std::error_code Ec;
	std::filesystem::create_directories(InOptions.OutputDirectory, Ec);
	if (Ec)
	{
		std::cerr << "Failed to create output directory: " << InOptions.OutputDirectory << " (" << Ec.message() << ")\n";
		return 1;
	}

	FClangAstScanner Scanner(InOptions.CompileCommandsPath);
	if (!Scanner.IsReady())
	{
		std::cerr << Scanner.GetLastError() << std::endl;
		return 1;
	}

	const std::vector<std::filesystem::path> Headers = CollectReflectionHeaders(InOptions.ScanDirectory);
	if (Headers.empty())
	{
		std::cout << "ReflectionGenerator: no reflection headers found under " << InOptions.ScanDirectory << std::endl;
		return 0;
	}

	int GeneratedCount = 0;
	for (const std::filesystem::path& HeaderPath : Headers)
	{
		std::string MacroError;
		const std::vector<FReflectionMacroRecord> MacroRecords = TokenizeAndParseReflectionMacros(HeaderPath, &MacroError);
		if (!MacroError.empty())
		{
			std::cerr << MacroError << std::endl;
			return 1;
		}

		std::vector<FReflectionClassRecord> ClassRecords = Scanner.ScanHeader(HeaderPath, MacroRecords);
		if (ClassRecords.empty())
		{
			std::cerr << "No UCLASS/USTRUCT declaration found in reflection header: " << HeaderPath << std::endl;
			return 1;
		}

		for (FReflectionClassRecord& ClassRecord : ClassRecords)
		{
			std::error_code RelativeEc;
			const std::filesystem::path RelativeHeaderPath =
				std::filesystem::relative(ClassRecord.HeaderPath, InOptions.ScanDirectory, RelativeEc);
			ClassRecord.HeaderIncludePath =
				RelativeEc ? ClassRecord.HeaderPath.filename().string() : RelativeHeaderPath.generic_string();
			AssociateReflectionMacros(&ClassRecord, MacroRecords);

			std::string EmitError;
			if (!EmitReflectionGeneratedFiles(ClassRecord, InOptions.OutputDirectory, &EmitError))
			{
				std::cerr << EmitError << std::endl;
				return 1;
			}

			++GeneratedCount;
			std::cout << "Generated reflection code for " << ClassRecord.ClassName
			          << " (" << HeaderPath.filename().string() << ")\n";
		}
	}

	std::cout << "ReflectionGenerator: generated " << GeneratedCount << " class(es).\n";
	return 0;
}
