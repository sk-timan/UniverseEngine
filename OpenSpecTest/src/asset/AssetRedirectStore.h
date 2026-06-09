#pragma once

#include <string>
#include <unordered_map>

class FAssetRedirectStore
{
public:
	static FAssetRedirectStore& Get();

	void Load();
	bool Save(std::string* OutErrorMessage = nullptr);
	void RecordRedirect(const std::string& InOldSoftObjectPath, const std::string& InNewSoftObjectPath);
	std::string ResolveRedirectChain(const std::string& InSoftObjectPath) const;

private:
	FAssetRedirectStore() = default;

	std::unordered_map<std::string, std::string> Redirects_;
};
