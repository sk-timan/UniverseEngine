#include "ui/AssetBrowserItemInteraction.h"

#include "asset/AssetTypeInfo.h"
#include "ui/AssetListModel.h"

namespace
{
FAssetBrowserItemCapabilities IntersectTwoCapabilities(
	const FAssetBrowserItemCapabilities& InLeft,
	const FAssetBrowserItemCapabilities& InRight)
{
	FAssetBrowserItemCapabilities Result;
	Result.bCanDelete = InLeft.bCanDelete && InRight.bCanDelete;
	Result.bCanRename = InLeft.bCanRename && InRight.bCanRename;
	Result.bCanCopy = InLeft.bCanCopy && InRight.bCanCopy;
	Result.bCanDuplicate = InLeft.bCanDuplicate && InRight.bCanDuplicate;
	Result.bCanReimport = InLeft.bCanReimport && InRight.bCanReimport;
	Result.bCanMove = InLeft.bCanMove && InRight.bCanMove;
	Result.bCanShowInExplorer = InLeft.bCanShowInExplorer && InRight.bCanShowInExplorer;
	Result.bCanCopySoftObjectPath = InLeft.bCanCopySoftObjectPath && InRight.bCanCopySoftObjectPath;
	Result.bCanCreateSubfolder = InLeft.bCanCreateSubfolder && InRight.bCanCreateSubfolder;
	return Result;
}

void ApplySelectionLevelCapabilityRules(FAssetBrowserGridSelectionState& InOutState)
{
	if (InOutState.bIsMultiSelection)
	{
		InOutState.Capabilities.bCanRename = false;
		InOutState.Capabilities.bCanShowInExplorer = false;
		InOutState.Capabilities.bCanCopySoftObjectPath = false;
		InOutState.Capabilities.bCanCreateSubfolder = false;
	}

	if (InOutState.bIsMixedTypeSelection)
	{
		InOutState.Capabilities.bCanCopy = false;
		InOutState.Capabilities.bCanDuplicate = false;
		InOutState.Capabilities.bCanReimport = false;
		InOutState.Capabilities.bCanShowInExplorer = false;
		InOutState.Capabilities.bCanCopySoftObjectPath = false;
		InOutState.Capabilities.bCanCreateSubfolder = false;
	}
}
}

FAssetBrowserItemCapabilities IntersectItemCapabilities(
	const std::vector<FAssetBrowserItemCapabilities>& InPerItemCapabilities)
{
	if (InPerItemCapabilities.empty())
	{
		return {};
	}

	FAssetBrowserItemCapabilities Result = InPerItemCapabilities.front();
	for (size_t Index = 1; Index < InPerItemCapabilities.size(); ++Index)
	{
		Result = IntersectTwoCapabilities(Result, InPerItemCapabilities[Index]);
	}

	return Result;
}

bool CanRenameFolderPathForInteraction(const std::string& InFolderPath)
{
	return !InFolderPath.empty() && InFolderPath != "All" && InFolderPath != "Content";
}

FAssetBrowserItemCapabilities GetFolderItemCapabilities(
	const std::string& InFolderPath,
	bool bAllowCreateSubfolder)
{
	FAssetBrowserItemCapabilities Capabilities;
	const bool bCanOperateFolder = CanRenameFolderPathForInteraction(InFolderPath);
	Capabilities.bCanDelete = bCanOperateFolder;
	Capabilities.bCanRename = bCanOperateFolder;
	Capabilities.bCanMove = bCanOperateFolder;
	Capabilities.bCanCreateSubfolder = bAllowCreateSubfolder && !InFolderPath.empty() && InFolderPath != "All";
	return Capabilities;
}

FAssetBrowserItemCapabilities GetAssetItemCapabilities(const FAssetRegistryEntry& InEntry)
{
	FAssetBrowserItemCapabilities Capabilities;
	Capabilities.bCanDelete = true;
	Capabilities.bCanRename = true;
	Capabilities.bCanCopy = true;
	Capabilities.bCanDuplicate = true;
	Capabilities.bCanMove = true;
	Capabilities.bCanShowInExplorer = true;
	Capabilities.bCanCopySoftObjectPath = true;
	Capabilities.bCanReimport = AssetTypeInfo::IsMeshAssetType(InEntry.Type);
	return Capabilities;
}

FAssetBrowserGridSelectionState BuildGridSelectionState(
	const std::vector<FAssetBrowserSelectedGridItem>& InItems)
{
	FAssetBrowserGridSelectionState State;
	State.Items = InItems;
	State.bIsMultiSelection = InItems.size() > 1;

	std::vector<FAssetBrowserItemCapabilities> PerItemCapabilities;
	PerItemCapabilities.reserve(InItems.size());
	for (const FAssetBrowserSelectedGridItem& Item : InItems)
	{
		if (Item.Kind == EAssetBrowserItemKind::Folder)
		{
			++State.FolderCount;
			PerItemCapabilities.push_back(GetFolderItemCapabilities(Item.FolderPath));
		}
		else
		{
			++State.AssetCount;
			if (Item.AssetListItem != nullptr)
			{
				PerItemCapabilities.push_back(GetAssetItemCapabilities(Item.AssetListItem->Entry));
			}
		}
	}

	State.bIsMixedTypeSelection = State.FolderCount > 0 && State.AssetCount > 0;
	State.Capabilities = IntersectItemCapabilities(PerItemCapabilities);
	ApplySelectionLevelCapabilityRules(State);
	return State;
}
