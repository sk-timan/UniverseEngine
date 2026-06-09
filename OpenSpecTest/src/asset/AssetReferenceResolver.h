#pragma once

#include <string>

struct FResolvedAssetReference
{
	std::string SoftObjectPath;
	std::string Guid;
	bool bResolvedByGuid = false;
	bool bUsedRedirect = false;
};

class FAssetReferenceResolver
{
public:
	static FResolvedAssetReference Resolve(
		const std::string& InGuid,
		const std::string& InSoftObjectPath);

	static std::string BuildSoftPathFromRegistryEntry(
		const std::string& InAssetPath,
		const std::string& InObjectName);
};
