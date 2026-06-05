#include "data/ResourceLoader.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <cstdlib>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "data/GameplayConfigStore.h"
#include "render/ResourceRegistry.h"
#include "render/asset/StaticMesh.h"
#include "render/asset/SkeletalMesh.h"

namespace
{
bool TryReadFloat(const nlohmann::json& InJson, const char* InKey, float* OutValue)
{
	if (OutValue == nullptr || !InJson.contains(InKey) || !InJson[InKey].is_number())
	{
		return false;
	}

	*OutValue = InJson[InKey].get<float>();
	return true;
}

bool ParseVector2(const nlohmann::json& InJson, FVector2D* OutValue)
{
	if (OutValue == nullptr || !InJson.is_object())
	{
		return false;
	}

	return TryReadFloat(InJson, "x", &OutValue->X) &&
		TryReadFloat(InJson, "y", &OutValue->Y);
}

bool ParseVector3(const nlohmann::json& InJson, FVector3* OutValue)
{
	if (OutValue == nullptr || !InJson.is_object())
	{
		return false;
	}

	return TryReadFloat(InJson, "x", &OutValue->X) &&
		TryReadFloat(InJson, "y", &OutValue->Y) &&
		TryReadFloat(InJson, "z", &OutValue->Z);
}

bool ParseVector4(const nlohmann::json& InJson, FVector4* OutValue)
{
	if (OutValue == nullptr || !InJson.is_object())
	{
		return false;
	}

	return TryReadFloat(InJson, "x", &OutValue->X) &&
		TryReadFloat(InJson, "y", &OutValue->Y) &&
		TryReadFloat(InJson, "z", &OutValue->Z) &&
		TryReadFloat(InJson, "w", &OutValue->W);
}

bool ParseBounds(const nlohmann::json& InJson, UStreamableRenderAsset::FBounds* OutBounds)
{
	if (OutBounds == nullptr || !InJson.is_object())
	{
		return false;
	}

	if (!InJson.contains("origin") || !InJson.contains("extent") || !InJson.contains("sphere_radius"))
	{
		return false;
	}

	if (!ParseVector3(InJson["origin"], &OutBounds->Origin) ||
		!ParseVector3(InJson["extent"], &OutBounds->Extent) ||
		!InJson["sphere_radius"].is_number())
	{
		return false;
	}

	OutBounds->SphereRadius = InJson["sphere_radius"].get<float>();
	return true;
}
} // namespace

bool ResourceLoader::Initialize(const std::filesystem::path& InDataRoot, std::string* OutErrorMessage)
{
	if (!std::filesystem::exists(InDataRoot) || !std::filesystem::is_directory(InDataRoot))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = BuildMissingPathMessage(InDataRoot);
		}
		return false;
	}

	const std::filesystem::path ConfigDir = InDataRoot / "config";
	const std::filesystem::path FishDir = InDataRoot / "fish";
	const std::filesystem::path ModelsDir = InDataRoot / "models";
	if (!std::filesystem::exists(ConfigDir) || !std::filesystem::is_directory(ConfigDir) ||
		!std::filesystem::exists(FishDir) || !std::filesystem::is_directory(FishDir) ||
		!std::filesystem::exists(ModelsDir) || !std::filesystem::is_directory(ModelsDir))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage =
				"Data layout is incomplete. Required folders: config/, fish/, models/. Root: " +
				InDataRoot.string();
		}
		return false;
	}

	m_data_root_ = InDataRoot;
	return true;
}

bool ResourceLoader::LoadJsonText(const std::filesystem::path& InRelativePath, std::string* OutText, std::string* OutErrorMessage) const
{
	const std::filesystem::path FullPath = m_data_root_ / InRelativePath;
	if (!ReadTextFile(FullPath, OutText, OutErrorMessage))
	{
		return false;
	}

	const std::string Trimmed = TrimLeadingAsciiWhitespace(*OutText);
	if (Trimmed.empty() || (Trimmed.front() != '{' && Trimmed.front() != '['))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "JSON placeholder validation failed: " + FullPath.string();
		}
		return false;
	}
	return true;
}

bool ResourceLoader::LoadCsvRows(const std::filesystem::path& InRelativePath, std::vector<std::string>* OutRows, std::string* OutErrorMessage) const
{
	if (OutRows == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "LoadCsvRows requires non-null out_rows.";
		}
		return false;
	}

	std::string Text;
	const std::filesystem::path FullPath = m_data_root_ / InRelativePath;
	if (!ReadTextFile(FullPath, &Text, OutErrorMessage))
	{
		return false;
	}

	OutRows->clear();
	std::istringstream Stream(Text);
	std::string Line;
	while (std::getline(Stream, Line))
	{
		if (!Line.empty() && Line.back() == '\r')
		{
			Line.pop_back();
		}
		if (!Line.empty())
		{
			OutRows->push_back(Line);
		}
	}

	if (OutRows->empty())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "CSV file is empty: " + FullPath.string();
		}
		return false;
	}
	if (OutRows->front().find(',') == std::string::npos)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "CSV header does not contain separators: " + FullPath.string();
		}
		return false;
	}
	return true;
}

bool ResourceLoader::LoadModelAsset(const std::filesystem::path& InRelativePath, ModelAsset* OutModelAsset, std::string* OutErrorMessage) const
{
	if (OutModelAsset == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "LoadModelAsset requires non-null out_model_asset.";
		}
		return false;
	}

	const std::filesystem::path FullPath = m_data_root_ / InRelativePath;
	if (!std::filesystem::exists(FullPath))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = BuildMissingPathMessage(FullPath);
		}
		return false;
	}

	const aiScene* Scene = aiImportFile(
		FullPath.string().c_str(),
		aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality |
			aiProcess_ValidateDataStructure);
	if (Scene == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Assimp failed to load model asset: " + FullPath.string() +
				". reason: " + std::string(aiGetErrorString());
		}
		return false;
	}
	if (Scene->mRootNode == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Model asset does not contain a valid scene graph: " + FullPath.string();
		}
		aiReleaseImport(Scene);
		return false;
	}

	ModelAsset ModelAssetData{};
	ModelAssetData.source_path = FullPath;
	ModelAssetData.mesh_count = Scene->mNumMeshes;
	ModelAssetData.material_count = Scene->mNumMaterials;

	uint32_t TotalVertexCount = 0;
	uint32_t TotalIndexCount = 0;
	for (uint32_t MeshIndex = 0; MeshIndex < Scene->mNumMeshes; ++MeshIndex)
	{
		const aiMesh* Mesh = Scene->mMeshes[MeshIndex];
		if (Mesh == nullptr)
		{
			continue;
		}
		TotalVertexCount += Mesh->mNumVertices;
		for (uint32_t FaceIndex = 0; FaceIndex < Mesh->mNumFaces; ++FaceIndex)
		{
			TotalIndexCount += Mesh->mFaces[FaceIndex].mNumIndices;
		}
	}

	uint32_t NodeCount = 0;
	std::vector<const aiNode*> Stack;
	Stack.push_back(Scene->mRootNode);
	while (!Stack.empty())
	{
		const aiNode* Node = Stack.back();
		Stack.pop_back();
		if (Node == nullptr)
		{
			continue;
		}
		++NodeCount;
		for (uint32_t ChildIndex = 0; ChildIndex < Node->mNumChildren; ++ChildIndex)
		{
			Stack.push_back(Node->mChildren[ChildIndex]);
		}
	}

	ModelAssetData.node_count = NodeCount;
	ModelAssetData.total_vertex_count = TotalVertexCount;
	ModelAssetData.total_index_count = TotalIndexCount;
	*OutModelAsset = ModelAssetData;
	aiReleaseImport(Scene);
	return true;
}

bool ResourceLoader::LoadGameplayConfig(const std::filesystem::path& InRelativePath, GameplayConfig* OutConfig, std::string* OutErrorMessage) const
{
	if (OutConfig == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "LoadGameplayConfig requires non-null out_config.";
		}
		return false;
	}

	const std::filesystem::path FullPath = m_data_root_ / InRelativePath;
	return GameplayConfigStore::LoadFromFile(FullPath, OutConfig, OutErrorMessage);
}

bool ResourceLoader::LoadFishSpeciesDefs(const std::filesystem::path& InRelativePath, std::vector<FishSpeciesDef>* OutSpeciesDefs, std::string* OutErrorMessage) const
{
	if (OutSpeciesDefs == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "LoadFishSpeciesDefs requires non-null out_species_defs.";
		}
		return false;
	}

	std::vector<std::string> Rows;
	if (!LoadCsvRows(InRelativePath, &Rows, OutErrorMessage))
	{
		return false;
	}

	const std::vector<std::string> Header = SplitCsvLine(Rows.front());
	if (Header.size() < 5 || Header[0] != "id" || Header[1] != "name" ||
		Header[2] != "minWeightKg" || Header[3] != "maxWeightKg" || Header[4] != "powerCurve")
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage =
				"Unexpected fish CSV header schema in: " + (m_data_root_ / InRelativePath).string();
		}
		return false;
	}

	OutSpeciesDefs->clear();
	OutSpeciesDefs->reserve(Rows.size() - 1);
	for (size_t Index = 1; Index < Rows.size(); ++Index)
	{
		const std::vector<std::string> Columns = SplitCsvLine(Rows[Index]);
		if (Columns.size() < 5)
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage =
					"Fish CSV row has insufficient columns at row " + std::to_string(Index + 1);
			}
			return false;
		}

		FishSpeciesDef Species{};
		Species.id = Columns[0];
		Species.name = Columns[1];
		Species.power_curve = Columns[4];
		if (Species.id.empty() || Species.name.empty() || Species.power_curve.empty() ||
			!ParseFloat(Columns[2], &Species.min_weight_kg) ||
			!ParseFloat(Columns[3], &Species.max_weight_kg) ||
			Species.min_weight_kg <= 0.0f ||
			Species.max_weight_kg < Species.min_weight_kg)
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage =
					"Fish CSV row validation failed at row " + std::to_string(Index + 1);
			}
			return false;
		}
		OutSpeciesDefs->push_back(Species);
	}

	return !OutSpeciesDefs->empty();
}

bool ResourceLoader::ReadTextFile(const std::filesystem::path& InPath, std::string* OutText, std::string* OutErrorMessage)
{
	if (OutText == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "ReadTextFile requires non-null out_text.";
		}
		return false;
	}
	if (!std::filesystem::exists(InPath))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = BuildMissingPathMessage(InPath);
		}
		return false;
	}

	std::ifstream File(InPath, std::ios::in | std::ios::binary);
	if (!File.is_open())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to open file: " + InPath.string();
		}
		return false;
	}

	std::ostringstream Buffer;
	Buffer << File.rdbuf();
	*OutText = Buffer.str();
	return true;
}

std::string ResourceLoader::TrimLeadingAsciiWhitespace(const std::string& InValue)
{
	size_t Index = 0;
	while (Index < InValue.size())
	{
		const char C = InValue[Index];
		if (C != ' ' && C != '\t' && C != '\r' && C != '\n')
		{
			break;
		}
		++Index;
	}
	return InValue.substr(Index);
}

std::string ResourceLoader::TrimAsciiWhitespace(const std::string& InValue)
{
	const std::string LeadingTrimmed = TrimLeadingAsciiWhitespace(InValue);
	if (LeadingTrimmed.empty())
	{
		return LeadingTrimmed;
	}

	size_t End = LeadingTrimmed.size();
	while (End > 0)
	{
		const char C = LeadingTrimmed[End - 1];
		if (C != ' ' && C != '\t' && C != '\r' && C != '\n')
		{
			break;
		}
		--End;
	}
	return LeadingTrimmed.substr(0, End);
}

std::vector<std::string> ResourceLoader::SplitCsvLine(const std::string& InLine)
{
	std::vector<std::string> Parts;
	std::string Current;
	for (const char C : InLine)
	{
		if (C == ',')
		{
			Parts.push_back(TrimAsciiWhitespace(Current));
			Current.clear();
			continue;
		}
		Current.push_back(C);
	}
	Parts.push_back(TrimAsciiWhitespace(Current));
	return Parts;
}

bool ResourceLoader::ParseFloat(const std::string& InText, float* OutValue)
{
	if (OutValue == nullptr)
	{
		return false;
	}

	const std::string Trimmed = TrimAsciiWhitespace(InText);
	if (Trimmed.empty())
	{
		return false;
	}

	char* EndPtr = nullptr;
	const float Value = std::strtof(Trimmed.c_str(), &EndPtr);
	if (EndPtr == nullptr || *EndPtr != '\0')
	{
		return false;
	}
	*OutValue = Value;
	return true;
}

std::string ResourceLoader::BuildMissingPathMessage(const std::filesystem::path& InFullPath)
{
	return "Path does not exist: " + InFullPath.string();
}

UStaticMesh* ResourceLoader::LoadStaticMesh(const std::filesystem::path& InRelativePath, std::string* OutErrorMessage)
{
	std::string Text;
	if (!LoadJsonText(InRelativePath, &Text, OutErrorMessage))
	{
		return nullptr;
	}

	try
	{
		const nlohmann::json Root = nlohmann::json::parse(Text);
		if (!Root.is_object())
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Static mesh asset root must be a JSON object: " + InRelativePath.string();
			}
			return nullptr;
		}

		if (!Root.contains("vertices") || !Root["vertices"].is_array())
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Static mesh asset is missing array field 'vertices': " + InRelativePath.string();
			}
			return nullptr;
		}
		if (!Root.contains("indices") || !Root["indices"].is_array())
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Static mesh asset is missing array field 'indices': " + InRelativePath.string();
			}
			return nullptr;
		}
		if (!Root.contains("sections") || !Root["sections"].is_array())
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Static mesh asset is missing array field 'sections': " + InRelativePath.string();
			}
			return nullptr;
		}

		UStaticMesh* Mesh = new UStaticMesh(0, InRelativePath.stem().string(), nullptr);
		Mesh->SetAssetPath(InRelativePath);

		std::vector<UStaticMesh::FVertex> Vertices;
		Vertices.reserve(Root["vertices"].size());
		for (size_t VertexIndex = 0; VertexIndex < Root["vertices"].size(); ++VertexIndex)
		{
			const nlohmann::json& VertexJson = Root["vertices"][VertexIndex];
			UStaticMesh::FVertex Vertex{};
			if (!VertexJson.is_object() ||
				!VertexJson.contains("position") ||
				!VertexJson.contains("normal") ||
				!VertexJson.contains("tex_coord") ||
				!VertexJson.contains("tangent") ||
				!ParseVector3(VertexJson["position"], &Vertex.Position) ||
				!ParseVector3(VertexJson["normal"], &Vertex.Normal) ||
				!ParseVector2(VertexJson["tex_coord"], &Vertex.TexCoord) ||
				!ParseVector4(VertexJson["tangent"], &Vertex.TangentVec))
			{
				delete Mesh;
				if (OutErrorMessage != nullptr)
				{
					*OutErrorMessage = "Static mesh vertex format is invalid at index " +
						std::to_string(VertexIndex) + ": " + InRelativePath.string();
				}
				return nullptr;
			}
			Vertices.push_back(Vertex);
		}
		Mesh->SetVertices(Vertices);

		std::vector<uint32_t> Indices;
		Indices.reserve(Root["indices"].size());
		for (size_t Index = 0; Index < Root["indices"].size(); ++Index)
		{
			const nlohmann::json& IndexJson = Root["indices"][Index];
			if (!IndexJson.is_number_unsigned())
			{
				delete Mesh;
				if (OutErrorMessage != nullptr)
				{
					*OutErrorMessage = "Static mesh index buffer contains non-unsigned entry at index " +
						std::to_string(Index) + ": " + InRelativePath.string();
				}
				return nullptr;
			}

			Indices.push_back(IndexJson.get<uint32_t>());
		}
		Mesh->SetIndices(Indices);

		for (size_t SectionIndex = 0; SectionIndex < Root["sections"].size(); ++SectionIndex)
		{
			const nlohmann::json& SectionJson = Root["sections"][SectionIndex];
			UStaticMesh::FStaticMeshSection Section{};
			if (!SectionJson.is_object() ||
				!SectionJson.contains("material_index") ||
				!SectionJson.contains("first_index") ||
				!SectionJson.contains("index_count") ||
				!SectionJson.contains("bounds") ||
				!SectionJson["material_index"].is_number_unsigned() ||
				!SectionJson["first_index"].is_number_unsigned() ||
				!SectionJson["index_count"].is_number_unsigned() ||
				!ParseBounds(SectionJson["bounds"], &Section.SectionBounds))
			{
				delete Mesh;
				if (OutErrorMessage != nullptr)
				{
					*OutErrorMessage = "Static mesh section format is invalid at index " +
						std::to_string(SectionIndex) + ": " + InRelativePath.string();
				}
				return nullptr;
			}

			Section.MaterialIndex = SectionJson["material_index"].get<uint32_t>();
			Section.FirstIndex = SectionJson["first_index"].get<uint32_t>();
			Section.IndexCount = SectionJson["index_count"].get<uint32_t>();
			Mesh->AddSection(Section);
		}

		RegisterLoadedAsset(Mesh);
		return Mesh;
	}
	catch (const std::exception& Exception)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to parse static mesh asset '" + InRelativePath.string() +
				"': " + Exception.what();
		}
		return nullptr;
	}
}

USkeletalMesh* ResourceLoader::LoadSkeletalMesh(const std::filesystem::path& InRelativePath, std::string* OutErrorMessage)
{
	std::string Text;
	if (!LoadJsonText(InRelativePath, &Text, OutErrorMessage))
	{
		return nullptr;
	}

	try
	{
		const nlohmann::json Root = nlohmann::json::parse(Text);
		if (!Root.is_object())
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Skeletal mesh asset root must be a JSON object: " + InRelativePath.string();
			}
			return nullptr;
		}

		if (!Root.contains("skeleton") || !Root["skeleton"].is_array())
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Skeletal mesh asset is missing array field 'skeleton': " + InRelativePath.string();
			}
			return nullptr;
		}
		if (!Root.contains("skin_vertices") || !Root["skin_vertices"].is_array())
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Skeletal mesh asset is missing array field 'skin_vertices': " + InRelativePath.string();
			}
			return nullptr;
		}
		if (!Root.contains("sections") || !Root["sections"].is_array())
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Skeletal mesh asset is missing array field 'sections': " + InRelativePath.string();
			}
			return nullptr;
		}

		USkeletalMesh* Mesh = new USkeletalMesh(0, InRelativePath.stem().string(), nullptr);
		Mesh->SetAssetPath(InRelativePath);

		std::vector<USkinnedAsset::FBone> Bones;
		Bones.reserve(Root["skeleton"].size());
		for (size_t BoneIndex = 0; BoneIndex < Root["skeleton"].size(); ++BoneIndex)
		{
			const nlohmann::json& BoneJson = Root["skeleton"][BoneIndex];
			if (!BoneJson.is_object() ||
				!BoneJson.contains("name") ||
				!BoneJson.contains("parent_index") ||
				!BoneJson["name"].is_string() ||
				!BoneJson["parent_index"].is_number_integer())
			{
				delete Mesh;
				if (OutErrorMessage != nullptr)
				{
					*OutErrorMessage = "Skeletal mesh bone format is invalid at index " +
						std::to_string(BoneIndex) + ": " + InRelativePath.string();
				}
				return nullptr;
			}

			USkinnedAsset::FBone Bone{};
			Bone.Name = BoneJson["name"].get<std::string>();
			Bone.ParentIndex = BoneJson["parent_index"].get<int32_t>();
			Bones.push_back(Bone);
		}
		Mesh->SetSkeleton(Bones);

		std::vector<USkinnedAsset::FSkinVertex> SkinVertices;
		SkinVertices.reserve(Root["skin_vertices"].size());
		for (size_t VertexIndex = 0; VertexIndex < Root["skin_vertices"].size(); ++VertexIndex)
		{
			const nlohmann::json& VertexJson = Root["skin_vertices"][VertexIndex];
			USkinnedAsset::FSkinVertex Vertex{};
			if (!VertexJson.is_object() ||
				!VertexJson.contains("position") ||
				!VertexJson.contains("normal") ||
				!VertexJson.contains("tex_coord") ||
				!VertexJson.contains("tangent") ||
				!VertexJson.contains("bone_indices") ||
				!VertexJson.contains("bone_weights") ||
				!ParseVector3(VertexJson["position"], &Vertex.Position) ||
				!ParseVector3(VertexJson["normal"], &Vertex.Normal) ||
				!ParseVector2(VertexJson["tex_coord"], &Vertex.TexCoord) ||
				!ParseVector4(VertexJson["tangent"], &Vertex.Tangent) ||
				!VertexJson["bone_indices"].is_array() ||
				!VertexJson["bone_weights"].is_array() ||
				VertexJson["bone_indices"].size() != 4 ||
				VertexJson["bone_weights"].size() != 4)
			{
				delete Mesh;
				if (OutErrorMessage != nullptr)
				{
					*OutErrorMessage = "Skeletal mesh skin vertex format is invalid at index " +
						std::to_string(VertexIndex) + ": " + InRelativePath.string();
				}
				return nullptr;
			}

			for (size_t WeightIndex = 0; WeightIndex < 4; ++WeightIndex)
			{
				if (!VertexJson["bone_indices"][WeightIndex].is_number_integer() ||
					!VertexJson["bone_weights"][WeightIndex].is_number())
				{
					delete Mesh;
					if (OutErrorMessage != nullptr)
					{
						*OutErrorMessage = "Skeletal mesh bone influence format is invalid at vertex " +
							std::to_string(VertexIndex) + ": " + InRelativePath.string();
					}
					return nullptr;
				}

				Vertex.BoneIndices[WeightIndex] = VertexJson["bone_indices"][WeightIndex].get<int32_t>();
				Vertex.BoneWeights[WeightIndex] = VertexJson["bone_weights"][WeightIndex].get<float>();
			}

			SkinVertices.push_back(Vertex);
		}
		Mesh->SetSkinVertices(SkinVertices);

		for (size_t SectionIndex = 0; SectionIndex < Root["sections"].size(); ++SectionIndex)
		{
			const nlohmann::json& SectionJson = Root["sections"][SectionIndex];
			if (!SectionJson.is_object() ||
				!SectionJson.contains("material_index") ||
				!SectionJson.contains("first_index") ||
				!SectionJson.contains("index_count") ||
				!SectionJson.contains("bone_indices") ||
				!SectionJson["material_index"].is_number_unsigned() ||
				!SectionJson["first_index"].is_number_unsigned() ||
				!SectionJson["index_count"].is_number_unsigned() ||
				!SectionJson["bone_indices"].is_array())
			{
				delete Mesh;
				if (OutErrorMessage != nullptr)
				{
					*OutErrorMessage = "Skeletal mesh section format is invalid at index " +
						std::to_string(SectionIndex) + ": " + InRelativePath.string();
				}
				return nullptr;
			}

			USkeletalMesh::FSkeletalMeshSection Section{};
			Section.MaterialIndex = SectionJson["material_index"].get<uint32_t>();
			Section.FirstIndex = SectionJson["first_index"].get<uint32_t>();
			Section.IndexCount = SectionJson["index_count"].get<uint32_t>();
			for (const nlohmann::json& BoneIndexJson : SectionJson["bone_indices"])
			{
				if (!BoneIndexJson.is_number_integer())
				{
					delete Mesh;
					if (OutErrorMessage != nullptr)
					{
						*OutErrorMessage = "Skeletal mesh section bone index is invalid at section " +
							std::to_string(SectionIndex) + ": " + InRelativePath.string();
					}
					return nullptr;
				}
				Section.BoneIndices.push_back(BoneIndexJson.get<int32_t>());
			}
			Mesh->AddSection(Section);
		}

		RegisterLoadedAsset(Mesh);
		return Mesh;
	}
	catch (const std::exception& Exception)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to parse skeletal mesh asset '" + InRelativePath.string() +
				"': " + Exception.what();
		}
		return nullptr;
	}
}

void ResourceLoader::RegisterLoadedAsset(UStreamableRenderAsset* InAsset)
{
	if (InAsset == nullptr)
	{
		return;
	}

	ResourceRegistry::Get().RegisterAsset(InAsset);
}
