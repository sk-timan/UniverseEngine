#include "reflection/Enum.h"

#include <utility>

UEnum::UEnum(std::string InEnumName, std::vector<std::pair<std::string, int64_t>> InValues)
	: EnumName_(std::move(InEnumName)),
	  Values_(std::move(InValues))
{
}

const std::string& UEnum::GetEnumName() const
{
	return EnumName_;
}

const std::vector<std::pair<std::string, int64_t>>& UEnum::GetValues() const
{
	return Values_;
}

std::string UEnum::GetNameByValue(int64_t InValue) const
{
	for (const auto& Entry : Values_)
	{
		if (Entry.second == InValue)
		{
			return Entry.first;
		}
	}
	return {};
}

std::optional<int64_t> UEnum::GetValueByName(const std::string& InName) const
{
	for (const auto& Entry : Values_)
	{
		if (Entry.first == InName)
		{
			return Entry.second;
		}
	}
	return std::nullopt;
}
