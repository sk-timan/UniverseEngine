#pragma once

#include <cstdint>

enum class EFunctionFlags : uint32_t
{
	None = 0,
	BlueprintCallable = 1 << 0,
	CallInEditor = 1 << 1,
};

constexpr EFunctionFlags operator|(EFunctionFlags InLeft, EFunctionFlags InRight)
{
	return static_cast<EFunctionFlags>(static_cast<uint32_t>(InLeft) | static_cast<uint32_t>(InRight));
}

constexpr bool HasAnyFunctionFlags(EFunctionFlags InFlags, EFunctionFlags InTest)
{
	return (static_cast<uint32_t>(InFlags) & static_cast<uint32_t>(InTest)) != 0;
}
