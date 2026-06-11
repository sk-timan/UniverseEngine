#include "render/asset/Texture.h"

#include <utility>

UTexture::UTexture(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
	: UStreamableRenderAsset(InObjectId, std::move(InObjectName), InClass)
{
}

UTexture::~UTexture() = default;

bool UTexture::GetSRGB() const
{
	return bSRGB_;
}

void UTexture::SetSRGB(bool bInSRGB)
{
	bSRGB_ = bInSRGB;
}

ETextureFilter UTexture::GetFilter() const
{
	return Filter_;
}

void UTexture::SetFilter(ETextureFilter InFilter)
{
	Filter_ = InFilter;
}

ETextureAddressMode UTexture::GetAddressU() const
{
	return AddressU_;
}

ETextureAddressMode UTexture::GetAddressV() const
{
	return AddressV_;
}

void UTexture::SetAddressMode(ETextureAddressMode InU, ETextureAddressMode InV)
{
	AddressU_ = InU;
	AddressV_ = InV;
}

bool UTexture::HasResidentPlatformData() const
{
	return false;
}

bool UTexture::HasResidentTextureResource() const
{
	return false;
}

void UTexture::InitResource(Dx12Renderer* InRenderer)
{
	(void)InRenderer;
}

void UTexture::ReleaseResource()
{
}
