#include "render/asset/Texture2D.h"

#include <cstring>
#include <utility>

#include <nlohmann/json.hpp>

#include "core/ObjectRegistry.h"
#include "render/Dx12Renderer.h"

namespace
{
std::unique_ptr<UTexture2D> CreateTexture2DInstance(uint64_t InObjectId, std::string InObjectName)
{
	return std::make_unique<UTexture2D>(InObjectId, std::move(InObjectName), &UTexture2D::StaticClass());
}

void AppendU32(std::string* OutBuffer, uint32_t InValue)
{
	OutBuffer->append(reinterpret_cast<const char*>(&InValue), sizeof(InValue));
}

void AppendBytes(std::string* OutBuffer, const void* InData, size_t InSize)
{
	if (InSize > 0)
	{
		OutBuffer->append(reinterpret_cast<const char*>(InData), InSize);
	}
}

void AppendLengthPrefixed(std::string* OutBuffer, const std::string& InText)
{
	AppendU32(OutBuffer, static_cast<uint32_t>(InText.size()));
	AppendBytes(OutBuffer, InText.data(), InText.size());
}
} // namespace

bool FBinaryTextureReader::ReadU32(uint32_t* OutValue)
{
	if (OutValue == nullptr || Cursor + sizeof(uint32_t) > Size)
	{
		return false;
	}
	std::memcpy(OutValue, Data + Cursor, sizeof(uint32_t));
	Cursor += sizeof(uint32_t);
	return true;
}

bool FBinaryTextureReader::ReadU8(uint8_t* OutValue)
{
	if (OutValue == nullptr || Cursor + 1 > Size)
	{
		return false;
	}
	*OutValue = static_cast<uint8_t>(Data[Cursor]);
	Cursor += 1;
	return true;
}

bool FBinaryTextureReader::ReadBytes(void* OutData, size_t InSize)
{
	if (Cursor + InSize > Size)
	{
		return false;
	}
	if (InSize > 0)
	{
		std::memcpy(OutData, Data + Cursor, InSize);
	}
	Cursor += InSize;
	return true;
}

bool FBinaryTextureReader::ReadLengthPrefixed(std::string* OutText)
{
	uint32_t Len = 0;
	if (!ReadU32(&Len) || Cursor + Len > Size)
	{
		return false;
	}
	OutText->assign(Data + Cursor, Len);
	Cursor += Len;
	return true;
}

UTexture2D::UTexture2D(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
	: UTexture(InObjectId, std::move(InObjectName), InClass != nullptr ? InClass : &UTexture2D::StaticClass())
{
}

UTexture2D::~UTexture2D()
{
	ReleaseResource();
}

const UClass& UTexture2D::StaticClass()
{
	static const UClass Class("UTexture2D", &UStreamableRenderAsset::StaticClass(), CreateTexture2DInstance);
	return Class;
}

uint32_t UTexture2D::GetSizeX() const
{
	return SizeX_;
}

uint32_t UTexture2D::GetSizeY() const
{
	return SizeY_;
}

uint32_t UTexture2D::GetMipCount() const
{
	return MipCount_;
}

const FTextureSource& UTexture2D::GetSource() const
{
	return Source_;
}

FTextureSource& UTexture2D::GetMutableSource()
{
	return Source_;
}

const FTexturePlatformData& UTexture2D::GetPlatformData() const
{
	return PlatformData_;
}

FTexturePlatformData& UTexture2D::GetMutablePlatformData()
{
	return PlatformData_;
}

void UTexture2D::SetSource(FTextureSource InSource)
{
	Source_ = std::move(InSource);
}

void UTexture2D::SetPlatformData(FTexturePlatformData InPlatformData)
{
	PlatformData_ = std::move(InPlatformData);
	SizeX_ = PlatformData_.Width;
	SizeY_ = PlatformData_.Height;
	MipCount_ = PlatformData_.MipCount;
}

bool UTexture2D::HasResidentPlatformData() const
{
	return PlatformData_.HasResidentData();
}

bool UTexture2D::HasResidentTextureResource() const
{
	return TextureResource_.IsValid();
}

void UTexture2D::InitResource(Dx12Renderer* InRenderer)
{
	if (InRenderer == nullptr || !HasResidentPlatformData())
	{
		return;
	}

	ReleaseResource();
	if (!InRenderer->UploadTexture2DResource(this, &TextureResource_))
	{
		TextureResource_.Reset();
	}
}

void UTexture2D::ReleaseResource()
{
	TextureResource_.Reset();
}

const FTextureResource& UTexture2D::GetTextureResource() const
{
	return TextureResource_;
}

void UTexture2D::Serialize(nlohmann::json* OutObjectJson) const
{
	UStreamableRenderAsset::Serialize(OutObjectJson);
	if (OutObjectJson == nullptr)
	{
		return;
	}

	(*OutObjectJson)["size_x"] = SizeX_;
	(*OutObjectJson)["size_y"] = SizeY_;
	(*OutObjectJson)["mip_count"] = MipCount_;
	(*OutObjectJson)["sRGB"] = GetSRGB();
}

UTexture2D* UTexture2D::DeserializeBinary(FBinaryTextureReader* InReader, std::string* OutErrorMessage)
{
	if (InReader == nullptr)
	{
		return nullptr;
	}

	std::string ObjectName;
	std::string AssetPath;
	if (!InReader->ReadLengthPrefixed(&ObjectName) || !InReader->ReadLengthPrefixed(&AssetPath))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Corrupt Texture2D uasset: identity";
		}
		return nullptr;
	}

	uint32_t Width = 0;
	uint32_t Height = 0;
	uint32_t MipCount = 0;
	uint8_t Format = 0;
	uint8_t SRGB = 1;
	uint8_t Filter = 0;
	uint8_t AddressU = 0;
	uint8_t AddressV = 0;
	if (!InReader->ReadU32(&Width) || !InReader->ReadU32(&Height) || !InReader->ReadU32(&MipCount) ||
		!InReader->ReadU8(&Format) || !InReader->ReadU8(&SRGB) || !InReader->ReadU8(&Filter) ||
		!InReader->ReadU8(&AddressU) || !InReader->ReadU8(&AddressV))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Corrupt Texture2D uasset: header fields";
		}
		return nullptr;
	}

	uint32_t SourceWidth = 0;
	uint32_t SourceHeight = 0;
	uint8_t SourceFormat = 0;
	uint32_t SourceBlobLen = 0;
	if (!InReader->ReadU32(&SourceWidth) || !InReader->ReadU32(&SourceHeight) ||
		!InReader->ReadU8(&SourceFormat) || !InReader->ReadU32(&SourceBlobLen))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Corrupt Texture2D uasset: source header";
		}
		return nullptr;
	}

	if (SourceBlobLen > 0)
	{
		std::vector<uint8_t> SourceBlob(SourceBlobLen);
		if (!InReader->ReadBytes(SourceBlob.data(), SourceBlobLen))
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Corrupt Texture2D uasset: source blob";
			}
			return nullptr;
		}
	}

	uint32_t PlatformMipCount = 0;
	if (!InReader->ReadU32(&PlatformMipCount))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Corrupt Texture2D uasset: platform mip count";
		}
		return nullptr;
	}

	FTexturePlatformData PlatformData;
	PlatformData.Width = Width;
	PlatformData.Height = Height;
	PlatformData.MipCount = PlatformMipCount;
	PlatformData.Format = static_cast<ETexturePixelFormat>(Format);
	PlatformData.Mips.reserve(PlatformMipCount);

	for (uint32_t MipIndex = 0; MipIndex < PlatformMipCount; ++MipIndex)
	{
		FTextureMipLevel Mip;
		uint32_t MipDataLen = 0;
		if (!InReader->ReadU32(&Mip.Width) || !InReader->ReadU32(&Mip.Height) ||
			!InReader->ReadU32(&MipDataLen))
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Corrupt Texture2D uasset: mip header";
			}
			return nullptr;
		}

		Mip.Data.resize(MipDataLen);
		if (MipDataLen > 0 && !InReader->ReadBytes(Mip.Data.data(), MipDataLen))
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Corrupt Texture2D uasset: mip data";
			}
			return nullptr;
		}
		PlatformData.Mips.push_back(std::move(Mip));
	}

	auto* Texture = new UTexture2D(0, ObjectName, nullptr);
	Texture->SetAssetPath(AssetPath);
	Texture->SetSRGB(SRGB != 0);
	Texture->SetFilter(static_cast<ETextureFilter>(Filter));
	Texture->SetAddressMode(static_cast<ETextureAddressMode>(AddressU), static_cast<ETextureAddressMode>(AddressV));
	Texture->SetPlatformData(std::move(PlatformData));
	return Texture;
}

UTexture2D* UTexture2D::Deserialize(const nlohmann::json& InObjectJson, std::string* OutErrorMessage)
{
	(void)OutErrorMessage;
	if (!InObjectJson.is_object())
	{
		return nullptr;
	}

	const std::string ObjectName = InObjectJson.value("object_name", "Texture");
	auto* Texture = new UTexture2D(0, ObjectName, nullptr);
	if (InObjectJson.contains("asset_path"))
	{
		Texture->SetAssetPath(InObjectJson.at("asset_path").get<std::string>());
	}
	if (InObjectJson.contains("sRGB"))
	{
		Texture->SetSRGB(InObjectJson.at("sRGB").get<bool>());
	}
	return Texture;
}

void UTexture2D::WriteBinaryPayload(std::string* OutBuffer) const
{
	if (OutBuffer == nullptr)
	{
		return;
	}

	AppendLengthPrefixed(OutBuffer, GetObjectName());
	AppendLengthPrefixed(OutBuffer, GetAssetPath().string());

	AppendU32(OutBuffer, SizeX_);
	AppendU32(OutBuffer, SizeY_);
	AppendU32(OutBuffer, MipCount_);
	AppendBytes(OutBuffer, &PlatformData_.Format, 1);
	const uint8_t SRGB = GetSRGB() ? 1 : 0;
	AppendBytes(OutBuffer, &SRGB, 1);
	const uint8_t Filter = static_cast<uint8_t>(GetFilter());
	AppendBytes(OutBuffer, &Filter, 1);
	const uint8_t AddressU = static_cast<uint8_t>(GetAddressU());
	const uint8_t AddressV = static_cast<uint8_t>(GetAddressV());
	AppendBytes(OutBuffer, &AddressU, 1);
	AppendBytes(OutBuffer, &AddressV, 1);

	AppendU32(OutBuffer, Source_.Width);
	AppendU32(OutBuffer, Source_.Height);
	const uint8_t SourceFormat = static_cast<uint8_t>(Source_.Format);
	AppendBytes(OutBuffer, &SourceFormat, 1);
	const uint32_t SourceBlobLen = 0;
	AppendU32(OutBuffer, SourceBlobLen);

	AppendU32(OutBuffer, static_cast<uint32_t>(PlatformData_.Mips.size()));
	for (const FTextureMipLevel& Mip : PlatformData_.Mips)
	{
		AppendU32(OutBuffer, Mip.Width);
		AppendU32(OutBuffer, Mip.Height);
		AppendU32(OutBuffer, static_cast<uint32_t>(Mip.Data.size()));
		AppendBytes(OutBuffer, Mip.Data.data(), Mip.Data.size());
	}
}
