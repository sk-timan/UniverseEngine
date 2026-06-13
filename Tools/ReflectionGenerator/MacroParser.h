#pragma once

#include "ReflectionTypes.h"

#include <filesystem>
#include <string>
#include <vector>

std::vector<FReflectionMacroRecord> TokenizeAndParseReflectionMacros(const std::filesystem::path& InHeaderPath, std::string* OutErrorMessage);

const FReflectionMacroRecord* FindMacroOnOrAboveLine(
	const std::vector<FReflectionMacroRecord>& InMacroRecords,
	int32_t InTargetLine,
	EReflectionMacroType InMacroType);

void AssociateReflectionMacros(FReflectionClassRecord* InOutClassRecord, const std::vector<FReflectionMacroRecord>& InMacroRecords);
