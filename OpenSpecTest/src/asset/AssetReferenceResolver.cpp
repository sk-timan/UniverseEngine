#include "asset/AssetReferenceResolver.h"

#include <algorithm>
#include <vector>

#include "asset/AssetRedirectStore.h"
#include "asset/AssetRegistry.h"
#include "asset/SoftObjectPath.h"

namespace
{
void AppendUniqueSoftPathCandidate(std::vector<std::string>* InOutCandidates, const std::string& InCandidate)
{
	if (InOutCandidates == nullptr || InCandidate.empty())
	{
		return;
	}

	if (std::find(InOutCandidates->begin(), InOutCandidates->end(), InCandidate) == InOutCandidates->end())
	{
		InOutCandidates->push_back(InCandidate);
	}
}

std::vector<std::string> BuildSoftPathCandidates(const std::string& InSoftObjectPath)
{
	std::vector<std::string> Candidates;
	AppendUniqueSoftPathCandidate(&Candidates, InSoftObjectPath);

	const FSoftObjectPath ParsedPath = FSoftObjectPath::Parse(InSoftObjectPath);
	const std::string CanonicalSoftPath =
		FSoftObjectPath::Build(ParsedPath.AssetPath, ParsedPath.ObjectName);
	AppendUniqueSoftPathCandidate(&Candidates, CanonicalSoftPath);

	const std::string RedirectedSoftPath =
		FAssetRedirectStore::Get().ResolveRedirectChain(InSoftObjectPath);
	AppendUniqueSoftPathCandidate(&Candidates, RedirectedSoftPath);

	const std::string RedirectedCanonicalSoftPath =
		FAssetRedirectStore::Get().ResolveRedirectChain(CanonicalSoftPath);
	AppendUniqueSoftPathCandidate(&Candidates, RedirectedCanonicalSoftPath);

	return Candidates;
}

bool TryResolveFromRegistryEntry(
	const FAssetRegistryEntry& InRegistryEntry,
	const std::string& InGuid,
	FResolvedAssetReference* OutResult)
{
	if (OutResult == nullptr)
	{
		return false;
	}

	OutResult->SoftObjectPath =
		FAssetReferenceResolver::BuildSoftPathFromRegistryEntry(
			InRegistryEntry.AssetPath,
			InRegistryEntry.ObjectName);
	if (OutResult->Guid.empty())
	{
		OutResult->Guid = InRegistryEntry.Guid;
	}
	else if (InGuid.empty())
	{
		OutResult->Guid = InRegistryEntry.Guid;
	}
	return true;
}
} // namespace

std::string FAssetReferenceResolver::BuildSoftPathFromRegistryEntry(
	const std::string& InAssetPath,
	const std::string& InObjectName)
{
	return FSoftObjectPath::Build(InAssetPath, InObjectName);
}

FResolvedAssetReference FAssetReferenceResolver::Resolve(
	const std::string& InGuid,
	const std::string& InSoftObjectPath)
{
	FResolvedAssetReference Result;
	Result.Guid = InGuid;

	if (!InGuid.empty())
	{
		if (const std::optional<FAssetRegistryEntry> RegistryEntry = FAssetRegistry::Get().FindByGuid(InGuid))
		{
			Result.SoftObjectPath =
				BuildSoftPathFromRegistryEntry(RegistryEntry->AssetPath, RegistryEntry->ObjectName);
			Result.Guid = RegistryEntry->Guid;
			Result.bResolvedByGuid = true;
			return Result;
		}
	}

	if (InSoftObjectPath.empty())
	{
		return Result;
	}

	const std::string RedirectedSoftPath =
		FAssetRedirectStore::Get().ResolveRedirectChain(InSoftObjectPath);
	Result.bUsedRedirect = RedirectedSoftPath != InSoftObjectPath;

	for (const std::string& CandidatePath : BuildSoftPathCandidates(InSoftObjectPath))
	{
		if (const std::optional<FAssetRegistryEntry> RegistryEntry =
				FAssetRegistry::Get().FindBySoftPath(CandidatePath))
		{
			(void)TryResolveFromRegistryEntry(*RegistryEntry, InGuid, &Result);
			if (CandidatePath != InSoftObjectPath && CandidatePath != FSoftObjectPath::Parse(InSoftObjectPath).ToString())
			{
				Result.bUsedRedirect = true;
			}
			return Result;
		}
	}

	const FSoftObjectPath ParsedPath = FSoftObjectPath::Parse(InSoftObjectPath);
	if (!ParsedPath.AssetPath.empty())
	{
		if (const std::optional<FAssetRegistryEntry> RegistryEntry =
				FAssetRegistry::Get().FindByAssetPath(ParsedPath.AssetPath))
		{
			(void)TryResolveFromRegistryEntry(*RegistryEntry, InGuid, &Result);
			Result.bUsedRedirect = true;
			return Result;
		}
	}

	Result.SoftObjectPath = RedirectedSoftPath;
	return Result;
}
