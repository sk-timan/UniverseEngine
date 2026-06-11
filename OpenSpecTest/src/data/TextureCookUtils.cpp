#include "data/TextureCookUtils.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

#include <algorithm>

namespace
{
uint32_t ComputeMipCount(uint32_t InWidth, uint32_t InHeight)
{
	uint32_t MipCount = 1;
	uint32_t Width = InWidth;
	uint32_t Height = InHeight;
	while (Width > 1 || Height > 1)
	{
		Width = std::max(1u, Width / 2u);
		Height = std::max(1u, Height / 2u);
		++MipCount;
	}
	return MipCount;
}

bool DownsampleMip(
	const FTextureMipLevel& InSourceMip,
	FTextureMipLevel* OutMip)
{
	if (OutMip == nullptr || InSourceMip.Width < 1 || InSourceMip.Height < 1)
	{
		return false;
	}

	const uint32_t TargetWidth = std::max(1u, InSourceMip.Width / 2u);
	const uint32_t TargetHeight = std::max(1u, InSourceMip.Height / 2u);
	OutMip->Width = TargetWidth;
	OutMip->Height = TargetHeight;
	OutMip->Data.resize(static_cast<size_t>(TargetWidth) * static_cast<size_t>(TargetHeight) * 4u);

	const unsigned char* Result = stbir_resize_uint8_linear(
		InSourceMip.Data.data(),
		static_cast<int>(InSourceMip.Width),
		static_cast<int>(InSourceMip.Height),
		0,
		OutMip->Data.data(),
		static_cast<int>(TargetWidth),
		static_cast<int>(TargetHeight),
		0,
		STBIR_RGBA);
	return Result != nullptr;
}
} // namespace

bool FTextureCookUtils::BuildPlatformData(
	const FTextureSource& InSource,
	int32_t InMaxSize,
	FTexturePlatformData* OutPlatformData,
	std::string* OutErrorMessage)
{
	if (OutPlatformData == nullptr || !InSource.IsValid())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Invalid texture source for cook.";
		}
		return false;
	}

	FTextureSource WorkingSource = InSource;
	if (InMaxSize > 0 &&
		(WorkingSource.Width > static_cast<uint32_t>(InMaxSize) ||
			WorkingSource.Height > static_cast<uint32_t>(InMaxSize)))
	{
		const float Scale = static_cast<float>(InMaxSize) /
			static_cast<float>(std::max(WorkingSource.Width, WorkingSource.Height));
		const int TargetWidth = std::max(1, static_cast<int>(WorkingSource.Width * Scale));
		const int TargetHeight = std::max(1, static_cast<int>(WorkingSource.Height * Scale));

		std::vector<uint8_t> Resized(static_cast<size_t>(TargetWidth) * static_cast<size_t>(TargetHeight) * 4u);
		const unsigned char* Result = stbir_resize_uint8_linear(
			WorkingSource.Pixels.data(),
			static_cast<int>(WorkingSource.Width),
			static_cast<int>(WorkingSource.Height),
			0,
			Resized.data(),
			TargetWidth,
			TargetHeight,
			0,
			STBIR_RGBA);
		if (Result == nullptr)
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Failed to resize texture during cook.";
			}
			return false;
		}

		WorkingSource.Width = static_cast<uint32_t>(TargetWidth);
		WorkingSource.Height = static_cast<uint32_t>(TargetHeight);
		WorkingSource.Pixels = std::move(Resized);
	}

	OutPlatformData->Width = WorkingSource.Width;
	OutPlatformData->Height = WorkingSource.Height;
	OutPlatformData->Format = ETexturePixelFormat::RGBA8;
	OutPlatformData->MipCount = ComputeMipCount(WorkingSource.Width, WorkingSource.Height);
	OutPlatformData->Mips.clear();
	OutPlatformData->Mips.reserve(OutPlatformData->MipCount);

	FTextureMipLevel Mip0;
	Mip0.Width = WorkingSource.Width;
	Mip0.Height = WorkingSource.Height;
	Mip0.Data = WorkingSource.Pixels;
	OutPlatformData->Mips.push_back(std::move(Mip0));

	for (uint32_t MipIndex = 1; MipIndex < OutPlatformData->MipCount; ++MipIndex)
	{
		FTextureMipLevel NextMip;
		if (!DownsampleMip(OutPlatformData->Mips[MipIndex - 1], &NextMip))
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Failed to generate texture mip chain.";
			}
			return false;
		}
		OutPlatformData->Mips.push_back(std::move(NextMip));
	}

	return OutPlatformData->HasResidentData();
}
