#include "asset/AssetFolderTree.h"

#include <algorithm>
#include <unordered_map>

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

		std::string CurrentPath;
		FAssetFolderNode* CurrentNode = ContentRaw;
		size_t Start = 0;
		while (Start < Entry.AssetPath.size())
		{
			const size_t SlashIndex = Entry.AssetPath.find('/', Start);
			const std::string Segment = (SlashIndex == std::string::npos)
				? Entry.AssetPath.substr(Start)
				: Entry.AssetPath.substr(Start, SlashIndex - Start);
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

	SortFolderChildren(&Root);
	return Root;
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
