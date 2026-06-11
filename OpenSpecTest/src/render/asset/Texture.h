#pragma once

#include <string>

#include "render/asset/StreamableRenderAsset.h"
#include "render/asset/TextureData.h"

class Dx12Renderer;

class UTexture : public UStreamableRenderAsset
{
public:
	UTexture(uint64_t InObjectId, std::string InObjectName, const UClass* InClass = nullptr);
	virtual ~UTexture();

	bool GetSRGB() const;
	void SetSRGB(bool bInSRGB);

	ETextureFilter GetFilter() const;
	void SetFilter(ETextureFilter InFilter);

	ETextureAddressMode GetAddressU() const;
	ETextureAddressMode GetAddressV() const;
	void SetAddressMode(ETextureAddressMode InU, ETextureAddressMode InV);

	virtual bool HasResidentPlatformData() const;
	virtual bool HasResidentTextureResource() const;

	virtual void InitResource(Dx12Renderer* InRenderer);
	virtual void ReleaseResource();

protected:
	bool bSRGB_ = true;
	ETextureFilter Filter_ = ETextureFilter::Linear;
	ETextureAddressMode AddressU_ = ETextureAddressMode::Wrap;
	ETextureAddressMode AddressV_ = ETextureAddressMode::Wrap;
};
