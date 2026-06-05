#include "data/EditorPerformanceStore.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

namespace
{
std::string Trim(const std::string& InText)
{
	const auto NotSpace = [](unsigned char InChar) { return !std::isspace(InChar); };
	auto Begin = std::find_if(InText.begin(), InText.end(), NotSpace);
	auto End = std::find_if(InText.rbegin(), InText.rend(), NotSpace).base();
	if (Begin >= End)
	{
		return {};
	}
	return std::string(Begin, End);
}

bool ParseSplitMethod(const std::string& InValue, EPickTriangleBvhSplitMethod* OutMethod)
{
	if (OutMethod == nullptr)
	{
		return false;
	}

	if (InValue == "Median" || InValue == "0")
	{
		*OutMethod = EPickTriangleBvhSplitMethod::Median;
		return true;
	}
	if (InValue == "Sah" || InValue == "SAH" || InValue == "1")
	{
		*OutMethod = EPickTriangleBvhSplitMethod::Sah;
		return true;
	}
	return false;
}

bool ApplyKeyValue(const std::string& InKey, const std::string& InValue, FEditorPerformanceSettings* OutSettings)
{
	if (OutSettings == nullptr)
	{
		return false;
	}

	if (InKey == "TriangleBvhSplitMethod")
	{
		return ParseSplitMethod(InValue, &OutSettings->TriangleBvhSplitMethod);
	}
	return false;
}

const char* SplitMethodToString(EPickTriangleBvhSplitMethod InMethod)
{
	return (InMethod == EPickTriangleBvhSplitMethod::Sah) ? "Sah" : "Median";
}
} // namespace

std::filesystem::path EditorPerformanceStore::GetDefaultIniPath(const std::filesystem::path& InExecutableDir)
{
	return InExecutableDir / "Config" / "EditorPerformance.ini";
}

bool EditorPerformanceStore::LoadFromFile(
	const std::filesystem::path& InFilePath,
	FEditorPerformanceSettings* OutSettings,
	std::string* OutErrorMessage)
{
	if (OutSettings == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "EditorPerformanceStore: OutSettings is null.";
		}
		return false;
	}

	if (!std::filesystem::exists(InFilePath))
	{
		return true;
	}

	std::ifstream File(InFilePath, std::ios::in);
	if (!File.is_open())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to open editor performance file: " + InFilePath.string();
		}
		return false;
	}

	std::string CurrentSection;
	std::string Line;
	while (std::getline(File, Line))
	{
		Line = Trim(Line);
		if (Line.empty() || Line[0] == ';' || Line[0] == '#')
		{
			continue;
		}

		if (Line.front() == '[' && Line.back() == ']')
		{
			CurrentSection = Line.substr(1, Line.size() - 2);
			continue;
		}

		if (CurrentSection != kIniSectionName)
		{
			continue;
		}

		const size_t EqualPos = Line.find('=');
		if (EqualPos == std::string::npos)
		{
			continue;
		}

		const std::string Key = Trim(Line.substr(0, EqualPos));
		const std::string Value = Trim(Line.substr(EqualPos + 1));
		if (!Key.empty())
		{
			(void)ApplyKeyValue(Key, Value, OutSettings);
		}
	}

	return Validate(*OutSettings, OutErrorMessage);
}

bool EditorPerformanceStore::SaveToFile(
	const std::filesystem::path& InFilePath,
	const FEditorPerformanceSettings& InSettings,
	std::string* OutErrorMessage)
{
	if (!Validate(InSettings, OutErrorMessage))
	{
		return false;
	}

	const std::filesystem::path ParentDir = InFilePath.parent_path();
	if (!ParentDir.empty() && !std::filesystem::exists(ParentDir))
	{
		std::error_code DirError;
		std::filesystem::create_directories(ParentDir, DirError);
		if (DirError)
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Failed to create editor performance directory: " + ParentDir.string();
			}
			return false;
		}
	}

	std::ofstream File(InFilePath, std::ios::out | std::ios::trunc);
	if (!File.is_open())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to open editor performance for writing: " + InFilePath.string();
		}
		return false;
	}

	File << '[' << kIniSectionName << ']' << '\n';
	File << "TriangleBvhSplitMethod=" << SplitMethodToString(InSettings.TriangleBvhSplitMethod) << '\n';
	return true;
}

bool EditorPerformanceStore::Validate(const FEditorPerformanceSettings& InSettings, std::string* OutErrorMessage)
{
	if (InSettings.TriangleBvhSplitMethod != EPickTriangleBvhSplitMethod::Median
		&& InSettings.TriangleBvhSplitMethod != EPickTriangleBvhSplitMethod::Sah)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "TriangleBvhSplitMethod must be Median or Sah.";
		}
		return false;
	}
	return true;
}
