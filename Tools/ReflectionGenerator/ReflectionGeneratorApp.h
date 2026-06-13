#pragma once

#include "ReflectionTypes.h"

#include <filesystem>
#include <string>
#include <vector>

bool ParseReflectionGeneratorOptions(int InArgc, char* InArgv[], FReflectionGeneratorOptions* OutOptions, std::string* OutErrorMessage);

int RunReflectionGenerator(const FReflectionGeneratorOptions& InOptions);
