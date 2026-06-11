#pragma once

#include <cstdint>
#include <string>

#include "render/asset/TextureData.h"

class FTextureCookUtils
{
public:
	static bool BuildPlatformData(
		const FTextureSource& InSource,
		int32_t InMaxSize,
		FTexturePlatformData* OutPlatformData,
		std::string* OutErrorMessage);
};
