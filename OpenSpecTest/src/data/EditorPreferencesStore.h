#pragma once

#include <filesystem>
#include <string>

#include "data/EditorPreferences.h"

class EditorPreferencesStore
{
public:
	static constexpr const char* kIniSectionName = "/Script/OpenSpecTest.EditorPreferences";

	static std::filesystem::path GetDefaultIniPath(const std::filesystem::path& InExecutableDir);
	static bool LoadFromFile(const std::filesystem::path& InFilePath, FEditorPreferences* OutPreferences, std::string* OutErrorMessage);
	static bool SaveToFile(const std::filesystem::path& InFilePath, const FEditorPreferences& InPreferences, std::string* OutErrorMessage);
	static bool Validate(const FEditorPreferences& InPreferences, std::string* OutErrorMessage);
};
