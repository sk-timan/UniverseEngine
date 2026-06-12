#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class UEnum
{
public:
	UEnum(std::string InEnumName, std::vector<std::pair<std::string, int64_t>> InValues);

	const std::string& GetEnumName() const;
	const std::vector<std::pair<std::string, int64_t>>& GetValues() const;

	std::string GetNameByValue(int64_t InValue) const;
	std::optional<int64_t> GetValueByName(const std::string& InName) const;

private:
	std::string EnumName_;
	std::vector<std::pair<std::string, int64_t>> Values_;
};
