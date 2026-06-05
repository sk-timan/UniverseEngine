#include "data/EditorPreferencesStore.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>

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

bool ParseFloat(const std::string& InText, float* OutValue)
{
	if (OutValue == nullptr)
	{
		return false;
	}
	try
	{
		const float Parsed = std::stof(InText);
		*OutValue = Parsed;
		return std::isfinite(Parsed);
	}
	catch (...)
	{
		return false;
	}
}

bool ApplyKeyValue(const std::string& InKey, const std::string& InValue, FEditorPreferences* OutPreferences)
{
	if (OutPreferences == nullptr)
	{
		return false;
	}

	if (InKey == "NearClipPlane")
	{
		return ParseFloat(InValue, &OutPreferences->NearClipPlane);
	}
	if (InKey == "FarClipPlane")
	{
		return ParseFloat(InValue, &OutPreferences->FarClipPlane);
	}
	if (InKey == "CameraMoveSpeed")
	{
		return ParseFloat(InValue, &OutPreferences->CameraMoveSpeed);
	}
	if (InKey == "CameraSpeedScalar")
	{
		return ParseFloat(InValue, &OutPreferences->CameraSpeedScalar);
	}
	return false;
}
} // namespace

std::filesystem::path EditorPreferencesStore::GetDefaultIniPath(const std::filesystem::path& InExecutableDir)
{
	return InExecutableDir / "Config" / "EditorPreferences.ini";
}

bool EditorPreferencesStore::LoadFromFile(
	const std::filesystem::path& InFilePath,
	FEditorPreferences* OutPreferences,
	std::string* OutErrorMessage)
{
	if (OutPreferences == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "EditorPreferencesStore: out_preferences is null.";
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
			*OutErrorMessage = "Failed to open editor preferences file: " + InFilePath.string();
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
			(void)ApplyKeyValue(Key, Value, OutPreferences);
		}
	}

	return Validate(*OutPreferences, OutErrorMessage);
}

bool EditorPreferencesStore::SaveToFile(
	const std::filesystem::path& InFilePath,
	const FEditorPreferences& InPreferences,
	std::string* OutErrorMessage)
{
	if (!Validate(InPreferences, OutErrorMessage))
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
				*OutErrorMessage = "Failed to create editor preferences directory: " + ParentDir.string();
			}
			return false;
		}
	}

	std::ofstream File(InFilePath, std::ios::out | std::ios::trunc);
	if (!File.is_open())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to open editor preferences for writing: " + InFilePath.string();
		}
		return false;
	}

	File << '[' << kIniSectionName << ']' << '\n';
	File << "NearClipPlane=" << InPreferences.NearClipPlane << '\n';
	File << "FarClipPlane=" << InPreferences.FarClipPlane << '\n';
	File << "CameraMoveSpeed=" << InPreferences.CameraMoveSpeed << '\n';
	File << "CameraSpeedScalar=" << InPreferences.CameraSpeedScalar << '\n';
	return true;
}

bool EditorPreferencesStore::Validate(const FEditorPreferences& InPreferences, std::string* OutErrorMessage)
{
	if (!std::isfinite(InPreferences.NearClipPlane) ||
		!std::isfinite(InPreferences.FarClipPlane) ||
		!std::isfinite(InPreferences.CameraMoveSpeed) ||
		!std::isfinite(InPreferences.CameraSpeedScalar))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Editor preferences contain non-finite values.";
		}
		return false;
	}

	if (InPreferences.NearClipPlane <= 0.0f)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "NearClipPlane must be greater than 0.";
		}
		return false;
	}

	if (InPreferences.FarClipPlane <= InPreferences.NearClipPlane)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "FarClipPlane must be greater than NearClipPlane.";
		}
		return false;
	}

	if (InPreferences.CameraMoveSpeed < 1.0f || InPreferences.CameraMoveSpeed > 32.0f)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "CameraMoveSpeed must be in range [1, 32].";
		}
		return false;
	}

	if (InPreferences.CameraSpeedScalar < 0.1f || InPreferences.CameraSpeedScalar > 10.0f)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "CameraSpeedScalar must be in range [0.1, 10].";
		}
		return false;
	}

	return true;
}
