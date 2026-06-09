#include "asset/AssetRedirectStore.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "asset/ProjectPaths.h"

namespace
{
constexpr int kRedirectStoreVersion = 1;
constexpr int kMaxRedirectChainDepth = 32;

std::filesystem::path GetRedirectStorePath()
{
	return GProjectContentDirectory / "AssetRedirects.json";
}
} // namespace

FAssetRedirectStore& FAssetRedirectStore::Get()
{
	static FAssetRedirectStore Instance;
	return Instance;
}

void FAssetRedirectStore::Load()
{
	Redirects_.clear();

	const std::filesystem::path StorePath = GetRedirectStorePath();
	if (!std::filesystem::exists(StorePath))
	{
		return;
	}

	try
	{
		std::ifstream InputStream(StorePath);
		nlohmann::json Root;
		InputStream >> Root;
		if (!Root.contains("redirects") || !Root.at("redirects").is_object())
		{
			return;
		}

		for (const auto& RedirectEntry : Root.at("redirects").items())
		{
			if (!RedirectEntry.value().is_string())
			{
				continue;
			}
			const std::string OldSoftPath = RedirectEntry.key();
			const std::string NewSoftPath = RedirectEntry.value().get<std::string>();
			if (!OldSoftPath.empty() && !NewSoftPath.empty() && OldSoftPath != NewSoftPath)
			{
				Redirects_[OldSoftPath] = NewSoftPath;
			}
		}
	}
	catch (...)
	{
		Redirects_.clear();
	}
}

bool FAssetRedirectStore::Save(std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "";
	}

	nlohmann::json RedirectObject = nlohmann::json::object();
	for (const auto& RedirectEntry : Redirects_)
	{
		RedirectObject[RedirectEntry.first] = RedirectEntry.second;
	}

	nlohmann::json Root = nlohmann::json{
		{"version", kRedirectStoreVersion},
		{"redirects", RedirectObject},
	};

	try
	{
		const std::filesystem::path StorePath = GetRedirectStorePath();
		std::error_code ErrorCode;
		std::filesystem::create_directories(StorePath.parent_path(), ErrorCode);

		std::ofstream OutputStream(StorePath, std::ios::trunc);
		if (!OutputStream.is_open())
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Failed to open redirect store for write: " + StorePath.string();
			}
			return false;
		}
		OutputStream << Root.dump(2);
		return true;
	}
	catch (const std::exception& Ex)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = std::string("Failed to save redirect store: ") + Ex.what();
		}
		return false;
	}
}

void FAssetRedirectStore::RecordRedirect(
	const std::string& InOldSoftObjectPath,
	const std::string& InNewSoftObjectPath)
{
	if (InOldSoftObjectPath.empty() || InNewSoftObjectPath.empty()
		|| InOldSoftObjectPath == InNewSoftObjectPath)
	{
		return;
	}

	Redirects_[InOldSoftObjectPath] = InNewSoftObjectPath;
	for (auto& RedirectEntry : Redirects_)
	{
		if (RedirectEntry.second == InOldSoftObjectPath)
		{
			RedirectEntry.second = InNewSoftObjectPath;
		}
	}

	(void)Save(nullptr);
}

std::string FAssetRedirectStore::ResolveRedirectChain(const std::string& InSoftObjectPath) const
{
	if (InSoftObjectPath.empty())
	{
		return InSoftObjectPath;
	}

	std::string CurrentPath = InSoftObjectPath;
	for (int Depth = 0; Depth < kMaxRedirectChainDepth; ++Depth)
	{
		const auto RedirectIt = Redirects_.find(CurrentPath);
		if (RedirectIt == Redirects_.end())
		{
			break;
		}
		if (RedirectIt->second == CurrentPath)
		{
			break;
		}
		CurrentPath = RedirectIt->second;
	}

	return CurrentPath;
}
