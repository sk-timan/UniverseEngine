#include "asset/AssetFolderTree.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <unordered_map>

#include "asset/ProjectPaths.h"

namespace
{
FAssetFolderNode* FindOrCreateChild(FAssetFolderNode* InParent, const std::string& InSegment, const std::string& InFullPath)
{
	for (const std::unique_ptr<FAssetFolderNode>& Child : InParent->Children)
	{
		if (Child->DisplayName == InSegment)
		{
			return Child.get();
		}
	}

	auto NewNode = std::make_unique<FAssetFolderNode>();
	NewNode->Path = InFullPath;
	NewNode->DisplayName = InSegment;
	FAssetFolderNode* RawNode = NewNode.get();
	InParent->Children.push_back(std::move(NewNode));
	return RawNode;
}

void SortFolderChildren(FAssetFolderNode* InNode)
{
	if (InNode == nullptr)
	{
		return;
	}

	std::sort(
		InNode->Children.begin(),
		InNode->Children.end(),
		[](const std::unique_ptr<FAssetFolderNode>& A, const std::unique_ptr<FAssetFolderNode>& B)
		{
			return A->DisplayName < B->DisplayName;
		});

	for (const std::unique_ptr<FAssetFolderNode>& Child : InNode->Children)
	{
		SortFolderChildren(Child.get());
	}
}

void MergeContentDirectories(FAssetFolderNode* InContentNode)
{
	if (InContentNode == nullptr || !std::filesystem::exists(GProjectContentDirectory))
	{
		return;
	}

	std::function<void(const std::filesystem::path&, FAssetFolderNode*)> WalkDirectory;
	WalkDirectory = [&](const std::filesystem::path& InDirectory, FAssetFolderNode* InParentNode)
	{
		std::error_code ErrorCode;
		for (const auto& DirectoryEntry : std::filesystem::directory_iterator(InDirectory, ErrorCode))
		{
			if (ErrorCode || !DirectoryEntry.is_directory())
			{
				continue;
			}

			const std::string Segment = FsPathComponentUtf8(DirectoryEntry.path());
			const std::filesystem::path RelativePath =
				std::filesystem::relative(DirectoryEntry.path(), GProjectContentDirectory, ErrorCode);
			if (ErrorCode || RelativePath.empty())
			{
				continue;
			}

			const std::string VirtualPath = FsPathUtf8Generic(RelativePath);
			FAssetFolderNode* ChildNode = FindOrCreateChild(InParentNode, Segment, VirtualPath);
			WalkDirectory(DirectoryEntry.path(), ChildNode);
		}
	};

	WalkDirectory(GProjectContentDirectory, InContentNode);
}
} // namespace

FAssetFolderNode AssetFolderTreeBuilder::BuildFromRegistry(const std::vector<FAssetRegistryEntry>& InEntries)
{
	FAssetFolderNode Root;
	Root.Path = "All";
	Root.DisplayName = "All";

	auto ContentNode = std::make_unique<FAssetFolderNode>();
	ContentNode->Path = "Content";
	ContentNode->DisplayName = "Content";
	FAssetFolderNode* ContentRaw = ContentNode.get();
	Root.Children.push_back(std::move(ContentNode));

	for (const FAssetRegistryEntry& Entry : InEntries)
	{
		if (Entry.AssetPath.empty())
		{
			continue;
		}

		const size_t LastSlashIndex = Entry.AssetPath.find_last_of('/');
		if (LastSlashIndex == std::string::npos)
		{
			continue;
		}

		const std::string FolderOnlyPath = Entry.AssetPath.substr(0, LastSlashIndex);
		if (FolderOnlyPath.empty())
		{
			continue;
		}

		std::string CurrentPath;
		FAssetFolderNode* CurrentNode = ContentRaw;
		size_t Start = 0;
		while (Start < FolderOnlyPath.size())
		{
			const size_t SlashIndex = FolderOnlyPath.find('/', Start);
			const std::string Segment = (SlashIndex == std::string::npos)
				? FolderOnlyPath.substr(Start)
				: FolderOnlyPath.substr(Start, SlashIndex - Start);
			if (Segment.empty())
			{
				break;
			}

			CurrentPath = CurrentPath.empty() ? Segment : CurrentPath + "/" + Segment;
			CurrentNode = FindOrCreateChild(CurrentNode, Segment, CurrentPath);
			if (SlashIndex == std::string::npos)
			{
				break;
			}
			Start = SlashIndex + 1;
		}
	}

	MergeContentDirectories(ContentRaw);
	SortFolderChildren(&Root);
	return Root;
}

bool AssetFolderTreeBuilder::IsAssetDirectChildOfFolder(
	const FAssetRegistryEntry& InEntry,
	const std::string& InFolderPath)
{
	if (InFolderPath.empty() || InFolderPath == "All")
	{
		return false;
	}

	if (InFolderPath == "Content")
	{
		return InEntry.AssetPath.find('/') == std::string::npos;
	}

	const std::string Prefix = InFolderPath + "/";
	if (InEntry.AssetPath.size() <= Prefix.size() || InEntry.AssetPath.compare(0, Prefix.size(), Prefix) != 0)
	{
		return false;
	}

	const std::string Remainder = InEntry.AssetPath.substr(Prefix.size());
	return !Remainder.empty() && Remainder.find('/') == std::string::npos;
}

bool AssetFolderTreeBuilder::IsAssetInFolder(
	const FAssetRegistryEntry& InEntry,
	const std::string& InFolderPath)
{
	if (InFolderPath.empty() || InFolderPath == "All" || InFolderPath == "Content")
	{
		return true;
	}

	const std::string& AssetPath = InEntry.AssetPath;
	if (AssetPath == InFolderPath)
	{
		return true;
	}

	const std::string Prefix = InFolderPath + "/";
	return AssetPath.size() > Prefix.size() && AssetPath.compare(0, Prefix.size(), Prefix) == 0;
}
