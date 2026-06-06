#include "asset/AssetRegistry.h"

#include "asset/AssetSerializer.h"
#include "asset/ProjectPaths.h"
#include "asset/SoftObjectPath.h"

namespace
{
bool DeriveRegistryPathsFromUAssetFile(
	const std::filesystem::path& InUAssetPath,
	std::string* OutAssetPath,
	std::string* OutObjectName)
{
	if (OutAssetPath == nullptr || OutObjectName == nullptr)
	{
		return false;
	}

	std::error_code ErrorCode;
	const std::filesystem::path RelativePath =
		std::filesystem::relative(InUAssetPath, GProjectContentDirectory, ErrorCode);
	if (ErrorCode || RelativePath.empty())
	{
		return false;
	}

	std::filesystem::path AssetPathPath = RelativePath;
	AssetPathPath.replace_extension("");
	*OutAssetPath = AssetPathPath.generic_string();
	*OutObjectName = AssetPathPath.filename().string();
	return !OutAssetPath->empty();
}

void UpsertRegistryEntry(FAssetRegistryEntry InEntry, std::vector<FAssetRegistryEntry>* InOutEntries)
{
	if (InOutEntries == nullptr)
	{
		return;
	}

	for (FAssetRegistryEntry& ExistingEntry : *InOutEntries)
	{
		if ((!InEntry.Guid.empty() && ExistingEntry.Guid == InEntry.Guid) ||
			ExistingEntry.AssetPath == InEntry.AssetPath)
		{
			ExistingEntry = std::move(InEntry);
			return;
		}
	}
	InOutEntries->push_back(std::move(InEntry));
}
} // namespace

FAssetRegistry& FAssetRegistry::Get()
{
	static FAssetRegistry Instance;
	return Instance;
}

void FAssetRegistry::ScanContentDirectory()
{
	Entries_.clear();
	if (!std::filesystem::exists(GProjectContentDirectory))
	{
		return;
	}

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(GProjectContentDirectory))
	{
		if (!Entry.is_regular_file())
		{
			continue;
		}
		if (Entry.path().extension() != ".uasset")
		{
			continue;
		}
		RegisterScannedUAsset(Entry.path());
	}
}

void FAssetRegistry::RegisterScannedUAsset(const std::filesystem::path& InUAssetPath)
{
	std::string AssetPath;
	std::string ObjectName;
	if (!DeriveRegistryPathsFromUAssetFile(InUAssetPath, &AssetPath, &ObjectName))
	{
		return;
	}

	FAssetRegistryEntry Entry;
	Entry.AssetPath = AssetPath;
	Entry.ObjectName = ObjectName;
	Entry.UAssetFilePath = InUAssetPath;

	const std::filesystem::path MetaPath = UAssetSerializer::GetMetaPathForUAsset(InUAssetPath);
	FAssetMeta Meta;
	if (UAssetSerializer::LoadMeta(MetaPath, &Meta, nullptr))
	{
		Entry.SourceFile = Meta.SourceFile;
	}

	UpsertRegistryEntry(std::move(Entry), &Entries_);
}

void FAssetRegistry::RegisterFromHeader(
	const FAssetPackageHeader& InHeader,
	const std::filesystem::path& InUAssetPath)
{
	FAssetRegistryEntry Entry;
	Entry.AssetPath = InHeader.AssetPath;
	Entry.ObjectName = InHeader.ObjectName;
	Entry.Type = InHeader.Type;
	Entry.Guid = InHeader.Guid;
	Entry.UAssetFilePath = InUAssetPath;
	Entry.DependsOn = InHeader.DependsOn;

	const std::filesystem::path MetaPath = UAssetSerializer::GetMetaPathForUAsset(InUAssetPath);
	FAssetMeta Meta;
	if (UAssetSerializer::LoadMeta(MetaPath, &Meta, nullptr))
	{
		Entry.SourceFile = Meta.SourceFile;
	}

	UpsertRegistryEntry(std::move(Entry), &Entries_);
}

void FAssetRegistry::RegisterFromDisk(const std::filesystem::path& InUAssetPath)
{
	FAssetPackageHeader Header;
	std::string ErrorMessage;
	if (!UAssetSerializer::LoadHeader(InUAssetPath, &Header, &ErrorMessage))
	{
		RegisterScannedUAsset(InUAssetPath);
		return;
	}

	RegisterFromHeader(Header, InUAssetPath);
}

std::optional<FAssetRegistryEntry> FAssetRegistry::FindBySoftPath(const std::string& InSoftPath) const
{
	const FSoftObjectPath SoftPath = FSoftObjectPath::Parse(InSoftPath);
	for (const FAssetRegistryEntry& Entry : Entries_)
	{
		if (Entry.AssetPath == SoftPath.AssetPath && Entry.ObjectName == SoftPath.ObjectName)
		{
			return Entry;
		}
	}
	return std::nullopt;
}

std::optional<FAssetRegistryEntry> FAssetRegistry::FindByAssetPath(const std::string& InAssetPath) const
{
	for (const FAssetRegistryEntry& Entry : Entries_)
	{
		if (Entry.AssetPath == InAssetPath)
		{
			return Entry;
		}
	}
	return std::nullopt;
}

std::vector<FAssetRegistryEntry> FAssetRegistry::ListAssets(const std::string& InTypeFilter) const
{
	if (InTypeFilter.empty())
	{
		return Entries_;
	}

	std::vector<FAssetRegistryEntry> Filtered;
	for (const FAssetRegistryEntry& Entry : Entries_)
	{
		if (Entry.Type == InTypeFilter)
		{
			Filtered.push_back(Entry);
		}
	}
	return Filtered;
}
