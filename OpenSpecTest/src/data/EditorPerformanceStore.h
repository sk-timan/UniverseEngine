#pragma once

#include <filesystem>
#include <string>

#include "editor/EditorPerformanceSettings.h"

class EditorPerformanceStore
{
public:
	static constexpr const char* kIniSectionName = "/Script/OpenSpecTest.EditorPerformance";

	static std::filesystem::path GetDefaultIniPath(const std::filesystem::path& InExecutableDir);
	static bool LoadFromFile(
		const std::filesystem::path& InFilePath,
		FEditorPerformanceSettings* OutSettings,
		std::string* OutErrorMessage);
	static bool SaveToFile(
		const std::filesystem::path& InFilePath,
		const FEditorPerformanceSettings& InSettings,
		std::string* OutErrorMessage);
	static bool Validate(const FEditorPerformanceSettings& InSettings, std::string* OutErrorMessage);
};
