#include "asset/AssetSerializer.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

#include <nlohmann/json.hpp>

#include "asset/AssetPackageHeader.h"
#include "render/asset/SkeletalMesh.h"
#include "render/asset/StaticMesh.h"
#include "render/asset/StreamableRenderAsset.h"
#include "render/asset/Texture2D.h"

namespace
{
bool WriteTextFile(const std::filesystem::path& InPath, const std::string& InText, std::string* OutErrorMessage)
{
	std::error_code ErrorCode;
	const std::filesystem::path Parent = InPath.parent_path();
	if (!Parent.empty())
	{
		std::filesystem::create_directories(Parent, ErrorCode);
	}

	std::ofstream OutputStream(InPath, std::ios::binary | std::ios::trunc);
	if (!OutputStream.is_open())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to open file for write: " + InPath.string();
		}
		return false;
	}
	OutputStream << InText;
	return true;
}

bool ReadTextFile(const std::filesystem::path& InPath, std::string* OutText, std::string* OutErrorMessage)
{
	if (OutText == nullptr)
	{
		return false;
	}

	std::ifstream InputStream(InPath, std::ios::binary);
	if (!InputStream.is_open())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to open file for read: " + InPath.string();
		}
		return false;
	}

	*OutText = std::string((std::istreambuf_iterator<char>(InputStream)), std::istreambuf_iterator<char>());
	return true;
}

// [payloadFormat(u8): 0=JSON text, 1=StaticMesh binary, 2=Texture2D binary][payload].
constexpr char kBinaryMagic[4] = {'U', 'A', 'S', 'T'};
constexpr uint32_t kBinaryVersion = 1;
constexpr uint8_t kPayloadJsonText = 0;
constexpr uint8_t kPayloadStaticMeshBinary = 1;
constexpr uint8_t kPayloadTexture2DBinary = 2;

void AppendU32(std::string* OutBuffer, uint32_t InValue)
{
	OutBuffer->append(reinterpret_cast<const char*>(&InValue), sizeof(InValue));
}

void AppendFloat(std::string* OutBuffer, float InValue)
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

bool HasBinaryMagic(const std::string& InText)
{
	return InText.size() >= sizeof(kBinaryMagic) && std::memcmp(InText.data(), kBinaryMagic, sizeof(kBinaryMagic)) == 0;
}

struct FBinaryReader
{
	const char* Data = nullptr;
	size_t Size = 0;
	size_t Cursor = 0;

	bool ReadU32(uint32_t* OutValue)
	{
		if (Cursor + sizeof(uint32_t) > Size)
		{
			return false;
		}
		std::memcpy(OutValue, Data + Cursor, sizeof(uint32_t));
		Cursor += sizeof(uint32_t);
		return true;
	}

	bool ReadU8(uint8_t* OutValue)
	{
		if (Cursor + 1 > Size)
		{
			return false;
		}
		*OutValue = static_cast<uint8_t>(Data[Cursor]);
		Cursor += 1;
		return true;
	}

	bool ReadFloat(float* OutValue)
	{
		if (Cursor + sizeof(float) > Size)
		{
			return false;
		}
		std::memcpy(OutValue, Data + Cursor, sizeof(float));
		Cursor += sizeof(float);
		return true;
	}

	bool ReadBytes(void* OutData, size_t InSize)
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

	bool ReadLengthPrefixed(std::string* OutText)
	{
		uint32_t Len = 0;
		if (!ReadU32(&Len))
		{
			return false;
		}
		if (Cursor + Len > Size)
		{
			return false;
		}
		OutText->assign(Data + Cursor, Len);
		Cursor += Len;
		return true;
	}
};

void WriteStaticMeshBinary(
	const UStaticMesh& InMesh,
	const std::string& InObjectName,
	const std::string& InAssetPath,
	std::string* OutBuffer)
{
	AppendLengthPrefixed(OutBuffer, InObjectName);
	AppendLengthPrefixed(OutBuffer, InAssetPath);

	const std::vector<UStaticMesh::FVertex>& Vertices = InMesh.GetVertices();
	AppendU32(OutBuffer, static_cast<uint32_t>(Vertices.size()));
	AppendBytes(OutBuffer, Vertices.data(), Vertices.size() * sizeof(UStaticMesh::FVertex));

	const std::vector<uint32_t>& Indices = InMesh.GetIndices();
	AppendU32(OutBuffer, static_cast<uint32_t>(Indices.size()));
	AppendBytes(OutBuffer, Indices.data(), Indices.size() * sizeof(uint32_t));

	const size_t SectionCount = InMesh.GetSectionCount();
	AppendU32(OutBuffer, static_cast<uint32_t>(SectionCount));
	for (size_t SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
	{
		const UStaticMesh::FStaticMeshSection& Section = InMesh.GetSection(SectionIndex);
		AppendU32(OutBuffer, Section.MaterialIndex);
		AppendU32(OutBuffer, Section.FirstIndex);
		AppendU32(OutBuffer, Section.IndexCount);
		AppendFloat(OutBuffer, Section.SectionBounds.Origin.X);
		AppendFloat(OutBuffer, Section.SectionBounds.Origin.Y);
		AppendFloat(OutBuffer, Section.SectionBounds.Origin.Z);
		AppendFloat(OutBuffer, Section.SectionBounds.Extent.X);
		AppendFloat(OutBuffer, Section.SectionBounds.Extent.Y);
		AppendFloat(OutBuffer, Section.SectionBounds.Extent.Z);
		AppendFloat(OutBuffer, Section.SectionBounds.SphereRadius);
	}
}

UStaticMesh* ReadStaticMeshBinary(FBinaryReader* InReader, std::string* OutErrorMessage)
{
	std::string ObjectName;
	std::string AssetPath;
	if (!InReader->ReadLengthPrefixed(&ObjectName) || !InReader->ReadLengthPrefixed(&AssetPath))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Corrupt binary uasset: identity";
		}
		return nullptr;
	}

	uint32_t VertexCount = 0;
	if (!InReader->ReadU32(&VertexCount))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Corrupt binary uasset: vertex count";
		}
		return nullptr;
	}
	std::vector<UStaticMesh::FVertex> Vertices(VertexCount);
	if (!InReader->ReadBytes(Vertices.data(), static_cast<size_t>(VertexCount) * sizeof(UStaticMesh::FVertex)))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Corrupt binary uasset: vertex data";
		}
		return nullptr;
	}

	uint32_t IndexCount = 0;
	if (!InReader->ReadU32(&IndexCount))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Corrupt binary uasset: index count";
		}
		return nullptr;
	}
	std::vector<uint32_t> Indices(IndexCount);
	if (!InReader->ReadBytes(Indices.data(), static_cast<size_t>(IndexCount) * sizeof(uint32_t)))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Corrupt binary uasset: index data";
		}
		return nullptr;
	}

	uint32_t SectionCount = 0;
	if (!InReader->ReadU32(&SectionCount))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Corrupt binary uasset: section count";
		}
		return nullptr;
	}

	UStaticMesh* StaticMesh = new UStaticMesh(0, ObjectName, nullptr);
	StaticMesh->SetAssetPath(AssetPath);
	StaticMesh->SetVertices(Vertices);
	StaticMesh->SetIndices(Indices);
	for (uint32_t SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
	{
		UStaticMesh::FStaticMeshSection Section;
		const bool bSectionOk =
			InReader->ReadU32(&Section.MaterialIndex) &&
			InReader->ReadU32(&Section.FirstIndex) &&
			InReader->ReadU32(&Section.IndexCount) &&
			InReader->ReadFloat(&Section.SectionBounds.Origin.X) &&
			InReader->ReadFloat(&Section.SectionBounds.Origin.Y) &&
			InReader->ReadFloat(&Section.SectionBounds.Origin.Z) &&
			InReader->ReadFloat(&Section.SectionBounds.Extent.X) &&
			InReader->ReadFloat(&Section.SectionBounds.Extent.Y) &&
			InReader->ReadFloat(&Section.SectionBounds.Extent.Z) &&
			InReader->ReadFloat(&Section.SectionBounds.SphereRadius);
		if (!bSectionOk)
		{
			delete StaticMesh;
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Corrupt binary uasset: section data";
			}
			return nullptr;
		}
		StaticMesh->AddSection(Section);
	}

	return StaticMesh;
}
} // namespace

bool UAssetSerializer::Save(
	const FAssetPackageHeader& InHeader,
	const UStreamableRenderAsset& InAsset,
	const std::filesystem::path& InUAssetPath,
	std::string* OutErrorMessage)
{
	nlohmann::json HeaderJson;
	InHeader.ToJson(&HeaderJson);
	const std::string HeaderText = HeaderJson.dump();

	std::string Buffer;
	Buffer.append(kBinaryMagic, sizeof(kBinaryMagic));
	AppendU32(&Buffer, kBinaryVersion);
	AppendLengthPrefixed(&Buffer, HeaderText);

	const UStaticMesh* StaticMesh = dynamic_cast<const UStaticMesh*>(&InAsset);
	if (StaticMesh != nullptr)
	{
		Buffer.push_back(static_cast<char>(kPayloadStaticMeshBinary));
		WriteStaticMeshBinary(*StaticMesh, InHeader.ObjectName, InHeader.AssetPath, &Buffer);
	}
	else if (const UTexture2D* Texture2D = dynamic_cast<const UTexture2D*>(&InAsset))
	{
		Buffer.push_back(static_cast<char>(kPayloadTexture2DBinary));
		const_cast<UTexture2D*>(Texture2D)->WriteBinaryPayload(&Buffer);
	}
	else
	{
		nlohmann::json ObjectJson;
		InAsset.Serialize(&ObjectJson);
		ObjectJson["object_name"] = InHeader.ObjectName;
		ObjectJson["asset_path"] = InHeader.AssetPath;

		Buffer.push_back(static_cast<char>(kPayloadJsonText));
		AppendLengthPrefixed(&Buffer, ObjectJson.dump());
	}

	return WriteTextFile(InUAssetPath, Buffer, OutErrorMessage);
}

bool UAssetSerializer::LoadHeader(
	const std::filesystem::path& InUAssetPath,
	FAssetPackageHeader* OutHeader,
	std::string* OutErrorMessage)
{
	if (OutHeader == nullptr)
	{
		return false;
	}

	std::string Text;
	if (!ReadTextFile(InUAssetPath, &Text, OutErrorMessage))
	{
		return false;
	}

	try
	{
		if (HasBinaryMagic(Text))
		{
			FBinaryReader Reader{Text.data(), Text.size(), sizeof(kBinaryMagic)};
			uint32_t Version = 0;
			std::string HeaderText;
			if (!Reader.ReadU32(&Version) || !Reader.ReadLengthPrefixed(&HeaderText))
			{
				if (OutErrorMessage != nullptr)
				{
					*OutErrorMessage = "Corrupt binary uasset header";
				}
				return false;
			}
			return OutHeader->FromJson(nlohmann::json::parse(HeaderText), OutErrorMessage);
		}

		const nlohmann::json PackageJson = nlohmann::json::parse(Text);
		if (!PackageJson.contains("header"))
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "uasset missing header";
			}
			return false;
		}
		return OutHeader->FromJson(PackageJson.at("header"), OutErrorMessage);
	}
	catch (const std::exception& Ex)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = std::string("Failed to parse uasset header: ") + Ex.what();
		}
		return false;
	}
}

UStreamableRenderAsset* UAssetSerializer::LoadObject(
	const std::filesystem::path& InUAssetPath,
	std::string* OutErrorMessage)
{
	std::string Text;
	if (!ReadTextFile(InUAssetPath, &Text, OutErrorMessage))
	{
		return nullptr;
	}

	try
	{
		if (HasBinaryMagic(Text))
		{
			FBinaryReader Reader{Text.data(), Text.size(), sizeof(kBinaryMagic)};
			uint32_t Version = 0;
			std::string HeaderText;
			if (!Reader.ReadU32(&Version) || !Reader.ReadLengthPrefixed(&HeaderText))
			{
				if (OutErrorMessage != nullptr)
				{
					*OutErrorMessage = "Corrupt binary uasset header";
				}
				return nullptr;
			}

			FAssetPackageHeader Header;
			if (!Header.FromJson(nlohmann::json::parse(HeaderText), OutErrorMessage))
			{
				return nullptr;
			}

			uint8_t PayloadFormat = 0;
			if (!Reader.ReadU8(&PayloadFormat))
			{
				if (OutErrorMessage != nullptr)
				{
					*OutErrorMessage = "Corrupt binary uasset payload format";
				}
				return nullptr;
			}

			if (PayloadFormat == kPayloadStaticMeshBinary)
			{
				return ReadStaticMeshBinary(&Reader, OutErrorMessage);
			}
			if (PayloadFormat == kPayloadTexture2DBinary)
			{
				FBinaryTextureReader TextureReader{};
				TextureReader.Data = Reader.Data;
				TextureReader.Size = Reader.Size;
				TextureReader.Cursor = Reader.Cursor;
				UTexture2D* Texture2D = UTexture2D::DeserializeBinary(&TextureReader, OutErrorMessage);
				Reader.Cursor = TextureReader.Cursor;
				return Texture2D;
			}

			std::string ObjectText;
			if (!Reader.ReadLengthPrefixed(&ObjectText))
			{
				if (OutErrorMessage != nullptr)
				{
					*OutErrorMessage = "Corrupt binary uasset json payload";
				}
				return nullptr;
			}
			nlohmann::json ObjectJson = nlohmann::json::parse(ObjectText);
			if (Header.Type == "SkeletalMesh")
			{
				return USkeletalMesh::Deserialize(ObjectJson, OutErrorMessage);
			}
			if (Header.Type == "StaticMesh")
			{
				return UStaticMesh::Deserialize(ObjectJson, OutErrorMessage);
			}
			if (Header.Type == "Texture2D")
			{
				return UTexture2D::Deserialize(ObjectJson, OutErrorMessage);
			}
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Unsupported uasset type: " + Header.Type;
			}
			return nullptr;
		}

		const nlohmann::json PackageJson = nlohmann::json::parse(Text);
		if (!PackageJson.contains("header") || !PackageJson.contains("object"))
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "uasset missing header or object";
			}
			return nullptr;
		}

		FAssetPackageHeader Header;
		if (!Header.FromJson(PackageJson.at("header"), OutErrorMessage))
		{
			return nullptr;
		}

		nlohmann::json ObjectJson = PackageJson.at("object");
		ObjectJson["object_name"] = Header.ObjectName;
		ObjectJson["asset_path"] = Header.AssetPath;

		if (Header.Type == "StaticMesh")
		{
			return UStaticMesh::Deserialize(ObjectJson, OutErrorMessage);
		}
		if (Header.Type == "SkeletalMesh")
		{
			return USkeletalMesh::Deserialize(ObjectJson, OutErrorMessage);
		}
		if (Header.Type == "Texture2D")
		{
			return UTexture2D::Deserialize(ObjectJson, OutErrorMessage);
		}

		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Unsupported uasset type: " + Header.Type;
		}
		return nullptr;
	}
	catch (const std::exception& Ex)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = std::string("Failed to parse uasset: ") + Ex.what();
		}
		return nullptr;
	}
}

bool UAssetSerializer::SaveMeta(
	const FAssetMeta& InMeta,
	const std::filesystem::path& InMetaPath,
	std::string* OutErrorMessage)
{
	nlohmann::json MetaJson;
	InMeta.ToJson(&MetaJson);
	return WriteTextFile(InMetaPath, MetaJson.dump(), OutErrorMessage);
}

bool UAssetSerializer::LoadMeta(
	const std::filesystem::path& InMetaPath,
	FAssetMeta* OutMeta,
	std::string* OutErrorMessage)
{
	if (OutMeta == nullptr)
	{
		return false;
	}

	std::string Text;
	if (!ReadTextFile(InMetaPath, &Text, OutErrorMessage))
	{
		return false;
	}

	try
	{
		return OutMeta->FromJson(nlohmann::json::parse(Text), OutErrorMessage);
	}
	catch (const std::exception& Ex)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = std::string("Failed to parse meta: ") + Ex.what();
		}
		return false;
	}
}

std::filesystem::path UAssetSerializer::GetMetaPathForUAsset(const std::filesystem::path& InUAssetPath)
{
	return InUAssetPath.string() + ".meta";
}
