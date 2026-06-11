#include "data/ImageDecoder.h"

#define STBI_WINDOWS_UTF8
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <cctype>

namespace
{
std::string ToLowerExtension(const std::filesystem::path& InPath)
{
	std::string Extension = InPath.extension().string();
	std::transform(Extension.begin(), Extension.end(), Extension.begin(),
		[](unsigned char InChar) { return static_cast<char>(std::tolower(InChar)); });
	return Extension;
}
} // namespace

bool FImageDecoder::IsSupportedImageExtension(const std::filesystem::path& InPath)
{
	const std::string Extension = ToLowerExtension(InPath);
	return Extension == ".png" || Extension == ".jpg" || Extension == ".jpeg" ||
		Extension == ".tga" || Extension == ".bmp" || Extension == ".hdr";
}

bool FImageDecoder::IsExrExtension(const std::filesystem::path& InPath)
{
	return ToLowerExtension(InPath) == ".exr";
}

bool FImageDecoder::DecodeFromFile(
	const std::filesystem::path& InFilePath,
	bool bInFlipY,
	FDecodedImage* OutImage,
	std::string* OutErrorMessage)
{
	if (OutImage == nullptr)
	{
		return false;
	}
	OutImage->Width = 0;
	OutImage->Height = 0;
	OutImage->Channels = 0;
	OutImage->Pixels.clear();

	if (IsExrExtension(InFilePath))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "EXR import is not supported yet: " + InFilePath.string();
		}
		return false;
	}

	if (!IsSupportedImageExtension(InFilePath))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Unsupported image format: " + InFilePath.string();
		}
		return false;
	}

	stbi_set_flip_vertically_on_load(bInFlipY ? 1 : 0);

	int Width = 0;
	int Height = 0;
	int ChannelsInFile = 0;
	unsigned char* RawPixels = stbi_load(InFilePath.string().c_str(), &Width, &Height, &ChannelsInFile, 4);
	if (RawPixels == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			const char* Reason = stbi_failure_reason();
			*OutErrorMessage = std::string("Failed to decode image: ") +
				(Reason != nullptr ? Reason : "unknown error");
		}
		return false;
	}

	OutImage->Width = static_cast<uint32_t>(Width);
	OutImage->Height = static_cast<uint32_t>(Height);
	OutImage->Channels = 4;
	const size_t PixelCount = static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4u;
	OutImage->Pixels.assign(RawPixels, RawPixels + PixelCount);
	stbi_image_free(RawPixels);
	return true;
}

bool FImageDecoder::DecodeFromMemory(
	const uint8_t* InBuffer,
	size_t InLength,
	bool bInFlipY,
	FDecodedImage* OutImage,
	std::string* OutErrorMessage)
{
	if (OutImage == nullptr || InBuffer == nullptr || InLength == 0)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Invalid memory decode input.";
		}
		return false;
	}

	stbi_set_flip_vertically_on_load(bInFlipY ? 1 : 0);

	int Width = 0;
	int Height = 0;
	int ChannelsInFile = 0;
	unsigned char* RawPixels = stbi_load_from_memory(InBuffer, static_cast<int>(InLength),
		&Width, &Height, &ChannelsInFile, 4);
	if (RawPixels == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			const char* Reason = stbi_failure_reason();
			*OutErrorMessage = std::string("Failed to decode image from memory: ") +
				(Reason != nullptr ? Reason : "unknown error");
		}
		return false;
	}

	OutImage->Width = static_cast<uint32_t>(Width);
	OutImage->Height = static_cast<uint32_t>(Height);
	OutImage->Channels = 4;
	const size_t PixelCount = static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4u;
	OutImage->Pixels.assign(RawPixels, RawPixels + PixelCount);
	stbi_image_free(RawPixels);
	return true;
}

FTextureSource FImageDecoder::ToTextureSource(const FDecodedImage& InImage)
{
	FTextureSource Source;
	Source.Width = InImage.Width;
	Source.Height = InImage.Height;
	Source.Format = ETexturePixelFormat::RGBA8;
	Source.Pixels = InImage.Pixels;
	return Source;
}
