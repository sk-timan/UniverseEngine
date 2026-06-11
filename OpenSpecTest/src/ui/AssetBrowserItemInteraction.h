#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "asset/AssetRegistry.h"

struct FAssetBrowserListItem;

enum class EAssetBrowserItemKind : uint8_t
{
	Folder,
	Asset,
};

struct FAssetBrowserItemCapabilities
{
	bool bCanDelete = false;
	bool bCanRename = false;
	bool bCanCopy = false;
	bool bCanDuplicate = false;
	bool bCanReimport = false;
	bool bCanMove = false;
	bool bCanShowInExplorer = false;
	bool bCanCopySoftObjectPath = false;
	bool bCanCreateSubfolder = false;
};

FAssetBrowserItemCapabilities IntersectItemCapabilities(
	const std::vector<FAssetBrowserItemCapabilities>& InPerItemCapabilities);

bool CanRenameFolderPathForInteraction(const std::string& InFolderPath);

FAssetBrowserItemCapabilities GetFolderItemCapabilities(
	const std::string& InFolderPath,
	bool bAllowCreateSubfolder = false);

FAssetBrowserItemCapabilities GetAssetItemCapabilities(const FAssetRegistryEntry& InEntry);

struct FAssetBrowserSelectedGridItem
{
	EAssetBrowserItemKind Kind = EAssetBrowserItemKind::Folder;
	std::string FolderPath;
	const FAssetBrowserListItem* AssetListItem = nullptr;
};

struct FAssetBrowserGridSelectionState
{
	std::vector<FAssetBrowserSelectedGridItem> Items;
	FAssetBrowserItemCapabilities Capabilities;
	int FolderCount = 0;
	int AssetCount = 0;
	bool bIsMixedTypeSelection = false;
	bool bIsMixedAssetTypeSelection = false;
	bool bIsMultiSelection = false;
};

FAssetBrowserGridSelectionState BuildGridSelectionState(
	const std::vector<FAssetBrowserSelectedGridItem>& InItems);
