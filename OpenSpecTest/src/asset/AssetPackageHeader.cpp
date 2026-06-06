#include "asset/AssetPackageHeader.h"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <random>
#include <sstream>

void FAssetPackageHeader::ToJson(nlohmann::json* OutJson) const
{
	if (OutJson == nullptr)
	{
		return;
	}

	*OutJson = nlohmann::json{
		{"magic", Magic},
		{"version", Version},
		{"guid", Guid},
		{"type", Type},
		{"asset_path", AssetPath},
		{"object_name", ObjectName},
		{"depends_on", DependsOn}
	};
}

bool FAssetPackageHeader::FromJson(const nlohmann::json& InJson, std::string* OutErrorMessage)
{
	if (!InJson.contains("magic") || InJson.at("magic").get<std::string>() != Magic)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Invalid uasset magic";
		}
		return false;
	}
	if (!InJson.contains("version") || InJson.at("version").get<int>() != Version)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Unsupported uasset version";
		}
		return false;
	}

	InJson.at("guid").get_to(Guid);
	InJson.at("type").get_to(Type);
	InJson.at("asset_path").get_to(AssetPath);
	InJson.at("object_name").get_to(ObjectName);
	if (InJson.contains("depends_on"))
	{
		InJson.at("depends_on").get_to(DependsOn);
	}
	else
	{
		DependsOn.clear();
	}
	return true;
}

void FAssetMeta::ToJson(nlohmann::json* OutJson) const
{
	if (OutJson == nullptr)
	{
		return;
	}

	*OutJson = nlohmann::json{
		{"source_file", SourceFile},
		{"source_timestamp", SourceTimestamp},
		{"import_settings", ImportSettings},
		{"import_hash", ImportHash}
	};
}

bool FAssetMeta::FromJson(const nlohmann::json& InJson, std::string* OutErrorMessage)
{
	(void)OutErrorMessage;
	if (InJson.contains("source_file"))
	{
		InJson.at("source_file").get_to(SourceFile);
	}
	if (InJson.contains("source_timestamp"))
	{
		InJson.at("source_timestamp").get_to(SourceTimestamp);
	}
	if (InJson.contains("import_settings"))
	{
		ImportSettings = InJson.at("import_settings");
	}
	if (InJson.contains("import_hash"))
	{
		InJson.at("import_hash").get_to(ImportHash);
	}
	return true;
}

std::string GenerateAssetGuid()
{
	static thread_local std::mt19937 RandomEngine{std::random_device{}()};
	std::uniform_int_distribution<int> Dist(0, 15);
	std::uniform_int_distribution<int> VariantDist(8, 11);

	auto Hex = [&Dist]() { return Dist(RandomEngine); };
	std::ostringstream Stream;
	Stream << std::hex;
	for (int Index = 0; Index < 32; ++Index)
	{
		if (Index == 8 || Index == 12 || Index == 16 || Index == 20)
		{
			Stream << '-';
		}
		if (Index == 12)
		{
			Stream << VariantDist(RandomEngine);
		}
		else
		{
			Stream << Hex() << Hex();
		}
	}
	return Stream.str();
}

std::string ComputeImportHash(const std::string& InSourceFile, const std::string& InSourceTimestamp)
{
	return InSourceFile + "|" + InSourceTimestamp;
}

std::string GetFileTimestampIso(const std::filesystem::path& InPath)
{
	if (!std::filesystem::exists(InPath))
	{
		return {};
	}

	const auto TimePoint = std::filesystem::last_write_time(InPath);
	const auto SystemTime = std::chrono::clock_cast<std::chrono::system_clock>(TimePoint);
	const std::time_t TimeT = std::chrono::system_clock::to_time_t(SystemTime);
	std::tm LocalTime{};
#if defined(_WIN32)
	localtime_s(&LocalTime, &TimeT);
#else
	localtime_r(&TimeT, &LocalTime);
#endif
	std::ostringstream Stream;
	Stream << std::put_time(&LocalTime, "%Y-%m-%dT%H:%M:%S");
	return Stream.str();
}
