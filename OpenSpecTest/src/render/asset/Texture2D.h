#pragma once

#include <string>

#include <nlohmann/json_fwd.hpp>

#include "core/UClass.h"
#include "render/asset/Texture.h"
#include "render/asset/TextureResource.h"

class UTexture2D : public UTexture
{
public:
	UTexture2D(uint64_t InObjectId, std::string InObjectName, const UClass* InClass = nullptr);
	~UTexture2D() override;

	static const UClass& StaticClass();

	uint32_t GetSizeX() const;
	uint32_t GetSizeY() const;
	uint32_t GetMipCount() const;

	const FTextureSource& GetSource() const;
	FTextureSource& GetMutableSource();

	const FTexturePlatformData& GetPlatformData() const;
	FTexturePlatformData& GetMutablePlatformData();

	void SetSource(FTextureSource InSource);
	void SetPlatformData(FTexturePlatformData InPlatformData);

	bool HasResidentPlatformData() const override;
	bool HasResidentTextureResource() const override;

	void InitResource(Dx12Renderer* InRenderer) override;
	void ReleaseResource() override;

	const FTextureResource& GetTextureResource() const;

	virtual void Serialize(nlohmann::json* OutObjectJson) const override;
	static UTexture2D* DeserializeBinary(class FBinaryTextureReader* InReader, std::string* OutErrorMessage);
	static UTexture2D* Deserialize(const nlohmann::json& InObjectJson, std::string* OutErrorMessage);

	void WriteBinaryPayload(std::string* OutBuffer) const;

private:
	uint32_t SizeX_ = 0;
	uint32_t SizeY_ = 0;
	uint32_t MipCount_ = 0;
	FTextureSource Source_;
	FTexturePlatformData PlatformData_;
	FTextureResource TextureResource_;
};

// Used by AssetSerializer anonymous reader; declared here for Texture2D::DeserializeBinary.
struct FBinaryTextureReader
{
	const char* Data = nullptr;
	size_t Size = 0;
	size_t Cursor = 0;

	bool ReadU32(uint32_t* OutValue);
	bool ReadU8(uint8_t* OutValue);
	bool ReadBytes(void* OutData, size_t InSize);
	bool ReadLengthPrefixed(std::string* OutText);
};
