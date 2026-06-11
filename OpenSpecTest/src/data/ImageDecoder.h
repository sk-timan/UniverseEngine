#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "render/asset/TextureData.h"

struct FDecodedImage
{
	uint32_t Width = 0;
	uint32_t Height = 0;
	uint32_t Channels = 0;
	std::vector<uint8_t> Pixels;
};

class FImageDecoder
{
public:
	static bool IsSupportedImageExtension(const std::filesystem::path& InPath);
	static bool IsExrExtension(const std::filesystem::path& InPath);

	static bool DecodeFromFile(
		const std::filesystem::path& InFilePath,
		bool bInFlipY,
		FDecodedImage* OutImage,
		std::string* OutErrorMessage);

	static bool DecodeFromMemory(
		const uint8_t* InBuffer,
		size_t InLength,
		bool bInFlipY,
		FDecodedImage* OutImage,
		std::string* OutErrorMessage);

	static FTextureSource ToTextureSource(const FDecodedImage& InImage);
};
