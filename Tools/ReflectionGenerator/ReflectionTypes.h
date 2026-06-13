#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

enum class EReflectionMacroType
{
	UClass,
	UStruct,
	UEnum,
	UProperty,
	UFunction
};

struct FReflectionMacroRecord
{
	EReflectionMacroType MacroType = EReflectionMacroType::UProperty;
	int32_t Line = 0;
	std::string RawArguments;
	std::vector<std::string> Flags;
	std::string Category;
};

struct FReflectionFieldRecord
{
	std::string Name;
	std::string TypeSpelling;
	int32_t Line = 0;
	std::size_t OffsetBytes = 0;
	std::vector<std::string> PropertyFlags;
	std::string Category;
	bool bIsReflected = false;
};

struct FReflectionMethodRecord
{
	std::string Name;
	std::string ReturnTypeSpelling;
	int32_t Line = 0;
	std::vector<std::string> FunctionFlags;
};

struct FReflectionClassRecord
{
	std::string ClassName;
	std::string SuperClassName;
	std::filesystem::path HeaderPath;
	std::string HeaderIncludePath;
	int32_t ClassLine = 0;
	std::vector<std::string> ClassFlags;
	std::vector<FReflectionFieldRecord> Fields;
	std::vector<FReflectionMethodRecord> Methods;
	bool bIsStruct = false;
};

struct FReflectionGeneratorOptions
{
	std::filesystem::path CompileCommandsPath;
	std::filesystem::path ScanDirectory;
	std::filesystem::path OutputDirectory;
	std::string ModuleName = "OpenSpecTest";
};
