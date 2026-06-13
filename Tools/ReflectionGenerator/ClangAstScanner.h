#pragma once

#include "ReflectionTypes.h"

#include <filesystem>
#include <string>
#include <vector>

class FClangAstScanner
{
public:
	explicit FClangAstScanner(std::filesystem::path InCompileCommandsPath);

	bool IsReady() const;
	const std::string& GetLastError() const;

	std::vector<FReflectionClassRecord> ScanHeader(const std::filesystem::path& InHeaderPath, const std::vector<FReflectionMacroRecord>& InMacroRecords);

private:
	std::filesystem::path CompileCommandsPath_;
	std::string LastError_;
};
