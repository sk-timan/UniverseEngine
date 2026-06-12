#pragma once

#include <cstddef>
#include <string>
#include <vector>

class UClass;

struct FGeneratedPropertyDesc
{
	std::string Name;
	std::string TypeSpelling;
	std::size_t OffsetBytes = 0;
	std::vector<std::string> PropertyFlags;
	std::string Category;
};

void RegisterGeneratedClassReflection(const UClass& InClass, const std::vector<FGeneratedPropertyDesc>& InProperties);
