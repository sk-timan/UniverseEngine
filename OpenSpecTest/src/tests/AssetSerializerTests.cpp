#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

#include "asset/AssetPackageHeader.h"
#include "asset/AssetSerializer.h"
#include "render/asset/StaticMesh.h"
#include "render/asset/StreamableRenderAsset.h"

namespace
{
bool AssertTrue(bool bCondition, const std::string& InMessage)
{
	if (!bCondition)
	{
		std::cerr << "ASSERT FAILED: " << InMessage << '\n';
		return false;
	}
	return true;
}

UStaticMesh* CreateTestStaticMesh()
{
	auto* Mesh = new UStaticMesh(0, "TestTriangle", nullptr);
	Mesh->SetAssetPath("Meshes/Test/TestTriangle");

	std::vector<UStaticMesh::FVertex> Vertices(3);
	Vertices[0].Position = {0.0f, 0.0f, 0.0f};
	Vertices[1].Position = {1.0f, 0.0f, 0.0f};
	Vertices[2].Position = {0.0f, 1.0f, 0.0f};
	for (auto& Vertex : Vertices)
	{
		Vertex.Normal = {0.0f, 0.0f, 1.0f};
	}
	Mesh->SetVertices(Vertices);
	Mesh->SetIndices({0, 1, 2});

	UStaticMesh::FStaticMeshSection Section;
	Section.MaterialIndex = 0;
	Section.FirstIndex = 0;
	Section.IndexCount = 3;
	Mesh->AddSection(Section);
	Mesh->RebuildSectionBounds();
	return Mesh;
}

bool TestStaticMeshRoundTrip()
{
	const std::filesystem::path TempDir =
		std::filesystem::temp_directory_path() / "openspec_asset_serializer_test";
	std::error_code ErrorCode;
	std::filesystem::remove_all(TempDir, ErrorCode);
	std::filesystem::create_directories(TempDir, ErrorCode);

	const std::filesystem::path UAssetPath = TempDir / "TestTriangle.uasset";
	const std::filesystem::path MetaPath = UAssetSerializer::GetMetaPathForUAsset(UAssetPath);

	std::unique_ptr<UStaticMesh> SourceMesh(CreateTestStaticMesh());

	FAssetPackageHeader Header;
	Header.Guid = GenerateAssetGuid();
	Header.Type = "StaticMesh";
	Header.AssetPath = "Meshes/Test/TestTriangle";
	Header.ObjectName = "TestTriangle";

	std::string Error;
	if (!AssertTrue(
			UAssetSerializer::Save(Header, *SourceMesh, UAssetPath, &Error),
			"Save uasset failed: " + Error))
	{
		return false;
	}

	FAssetMeta Meta;
	Meta.SourceFile = "C:/source/test.fbx";
	Meta.SourceTimestamp = "2026-01-01T00:00:00Z";
	Meta.ImportHash = ComputeImportHash(Meta.SourceFile, Meta.SourceTimestamp);
	if (!AssertTrue(
			UAssetSerializer::SaveMeta(Meta, MetaPath, &Error),
			"Save meta failed: " + Error))
	{
		return false;
	}

	FAssetPackageHeader LoadedHeader;
	if (!AssertTrue(
			UAssetSerializer::LoadHeader(UAssetPath, &LoadedHeader, &Error),
			"LoadHeader failed: " + Error))
	{
		return false;
	}
	if (!AssertTrue(LoadedHeader.Guid == Header.Guid, "Header guid mismatch"))
	{
		return false;
	}
	if (!AssertTrue(LoadedHeader.AssetPath == Header.AssetPath, "Header asset_path mismatch"))
	{
		return false;
	}

	FAssetMeta LoadedMeta;
	if (!AssertTrue(
			UAssetSerializer::LoadMeta(MetaPath, &LoadedMeta, &Error),
			"LoadMeta failed: " + Error))
	{
		return false;
	}
	if (!AssertTrue(LoadedMeta.SourceFile == Meta.SourceFile, "Meta source_file mismatch"))
	{
		return false;
	}

	std::unique_ptr<UStreamableRenderAsset> LoadedAsset(
		UAssetSerializer::LoadObject(UAssetPath, &Error));
	auto* LoadedMesh = dynamic_cast<UStaticMesh*>(LoadedAsset.get());
	if (!AssertTrue(LoadedMesh != nullptr, "LoadObject did not return StaticMesh: " + Error))
	{
		return false;
	}
	if (!AssertTrue(LoadedMesh->GetVertices().size() == 3, "Vertex count mismatch"))
	{
		return false;
	}
	if (!AssertTrue(LoadedMesh->GetIndices().size() == 3, "Index count mismatch"))
	{
		return false;
	}
	if (!AssertTrue(LoadedMesh->GetSectionCount() == 1, "Section count mismatch"))
	{
		return false;
	}
	if (!AssertTrue(
			std::abs(LoadedMesh->GetBounds().SphereRadius - SourceMesh->GetBounds().SphereRadius) < 1e-4f,
			"Bounds mismatch"))
	{
		return false;
	}

	std::filesystem::remove_all(TempDir, ErrorCode);
	return true;
}
} // namespace

int main()
{
	if (!TestStaticMeshRoundTrip())
	{
		return 1;
	}

	std::cout << "AssetSerializerTests passed.\n";
	return 0;
}
