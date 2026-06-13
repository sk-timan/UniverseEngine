#pragma once

#include "ReflectionTypes.h"

#include <filesystem>
#include <string>

bool EmitReflectionGeneratedFiles(const FReflectionClassRecord& InClassRecord, const std::filesystem::path& InOutputDirectory, std::string* OutErrorMessage);
