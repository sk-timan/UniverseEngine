#pragma once

#include <cstdint>

enum class EPropertyFlags : uint32_t
{
	None = 0,
	EditAnywhere = 1 << 0,
	VisibleAnywhere = 1 << 1,
	SaveGame = 1 << 2,
	Transient = 1 << 3,
};

constexpr EPropertyFlags operator|(EPropertyFlags InLeft, EPropertyFlags InRight)
{
	return static_cast<EPropertyFlags>(static_cast<uint32_t>(InLeft) | static_cast<uint32_t>(InRight));
}

constexpr EPropertyFlags operator&(EPropertyFlags InLeft, EPropertyFlags InRight)
{
	return static_cast<EPropertyFlags>(static_cast<uint32_t>(InLeft) & static_cast<uint32_t>(InRight));
}

constexpr bool HasAnyPropertyFlags(EPropertyFlags InFlags, EPropertyFlags InTest)
{
	return (InFlags & InTest) != EPropertyFlags::None;
}
