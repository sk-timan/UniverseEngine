#include "asset/SoftObjectPath.h"

#include <stdexcept>

FSoftObjectPath FSoftObjectPath::Parse(const std::string& InSoftPath)
{
	FSoftObjectPath Result;
	if (InSoftPath.empty())
	{
		return Result;
	}

	const size_t DotIndex = InSoftPath.rfind('.');
	if (DotIndex == std::string::npos || DotIndex == 0 || DotIndex + 1 >= InSoftPath.size())
	{
		Result.AssetPath = InSoftPath;
		const size_t SlashIndex = InSoftPath.rfind('/');
		if (SlashIndex != std::string::npos && SlashIndex + 1 < InSoftPath.size())
		{
			Result.ObjectName = InSoftPath.substr(SlashIndex + 1);
		}
		else
		{
			Result.ObjectName = InSoftPath;
		}
		return Result;
	}

	Result.AssetPath = InSoftPath.substr(0, DotIndex);
	Result.ObjectName = InSoftPath.substr(DotIndex + 1);
	return Result;
}

std::string FSoftObjectPath::Build(const std::string& InAssetPath, const std::string& InObjectName)
{
	if (InAssetPath.empty())
	{
		return InObjectName;
	}
	if (InObjectName.empty())
	{
		return InAssetPath;
	}
	return InAssetPath + "." + InObjectName;
}

std::string FSoftObjectPath::ToString() const
{
	return Build(AssetPath, ObjectName);
}

std::filesystem::path FSoftObjectPath::ToUAssetRelativePath() const
{
	if (AssetPath.empty())
	{
		return std::filesystem::path(ObjectName + ".uasset");
	}
	return std::filesystem::path(AssetPath + ".uasset");
}
