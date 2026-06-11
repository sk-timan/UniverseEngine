#pragma once

#include <cstdint>
#include <vector>

enum class ETexturePixelFormat : uint8_t
{
	RGBA8 = 0
};

enum class ETextureFilter : uint8_t
{
	Linear = 0,
	Point = 1
};

enum class ETextureAddressMode : uint8_t
{
	Wrap = 0,
	Clamp = 1
};

struct FTextureImportSettings
{
	bool bSRGB = true;
	bool bFlipY = false;
	int32_t MaxSize = 4096;
};

struct FTextureSource
{
	uint32_t Width = 0;
	uint32_t Height = 0;
	ETexturePixelFormat Format = ETexturePixelFormat::RGBA8;
	std::vector<uint8_t> Pixels;

	bool IsValid() const
	{
		return Width > 0 && Height > 0 &&
			Pixels.size() == static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4u;
	}
};

struct FTextureMipLevel
{
	uint32_t Width = 0;
	uint32_t Height = 0;
	std::vector<uint8_t> Data;
};

struct FTexturePlatformData
{
	uint32_t Width = 0;
	uint32_t Height = 0;
	uint32_t MipCount = 0;
	ETexturePixelFormat Format = ETexturePixelFormat::RGBA8;
	std::vector<FTextureMipLevel> Mips;

	bool HasResidentData() const
	{
		return Width > 0 && Height > 0 && MipCount > 0 && Mips.size() == MipCount;
	}

	const FTextureMipLevel* GetMip(uint32_t InMipIndex) const
	{
		if (InMipIndex >= Mips.size())
		{
			return nullptr;
		}
		return &Mips[InMipIndex];
	}
};
