#include "data/MeshImporter.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/matrix4x4.h>

#include <memory>
#include <unordered_map>

#include "data/AssimpCoordinate.h"
#include "render/asset/StaticMesh.h"
#include "render/asset/SkeletalMesh.h"
#include "render/ResourceRegistry.h"

namespace
{
bool SceneHasSkeletalContent(const aiScene* InScene)
{
	if (InScene == nullptr)
	{
		return false;
	}

	if (InScene->mNumAnimations > 0)
	{
		return true;
	}

	for (unsigned int MeshIndex = 0; MeshIndex < InScene->mNumMeshes; ++MeshIndex)
	{
		const aiMesh* AiMesh = InScene->mMeshes[MeshIndex];
		if (AiMesh != nullptr && AiMesh->mNumBones > 0)
		{
			return true;
		}
	}

	return false;
}

void SetupDefaultImport()
{
	aiPropertyStore* Store = aiCreatePropertyStore();
	aiSetImportPropertyInteger(Store, AI_CONFIG_PP_SLM_VERTEX_LIMIT, 65000);
	aiSetImportPropertyInteger(Store, AI_CONFIG_PP_ICL_PTCACHE_SIZE, 65536);
	aiReleasePropertyStore(Store);
}

void CollectMeshNodeTransforms(
	const aiNode* InNode,
	const aiMatrix4x4& InParentTransform,
	std::unordered_map<unsigned int, aiMatrix4x4>* OutMeshTransforms)
{
	if (InNode == nullptr || OutMeshTransforms == nullptr)
	{
		return;
	}

	const aiMatrix4x4 NodeTransform = InParentTransform * InNode->mTransformation;
	for (unsigned int MeshIndex = 0; MeshIndex < InNode->mNumMeshes; ++MeshIndex)
	{
		(*OutMeshTransforms)[InNode->mMeshes[MeshIndex]] = NodeTransform;
	}

	for (unsigned int ChildIndex = 0; ChildIndex < InNode->mNumChildren; ++ChildIndex)
	{
		CollectMeshNodeTransforms(InNode->mChildren[ChildIndex], NodeTransform, OutMeshTransforms);
	}
}

aiVector3D ToAiVector(const FVector3& InVector)
{
	return aiVector3D(InVector.X, InVector.Y, InVector.Z);
}

template<typename TVertexType>
void NormalizeTriangleWinding(const std::vector<TVertexType>& InVertices, std::vector<uint32_t>* InOutIndices)
{
	if (InOutIndices == nullptr || InOutIndices->size() < 3)
	{
		return;
	}

	for (size_t IndexOffset = 0; IndexOffset + 2 < InOutIndices->size(); IndexOffset += 3)
	{
		const uint32_t Index0 = (*InOutIndices)[IndexOffset];
		const uint32_t Index1 = (*InOutIndices)[IndexOffset + 1];
		const uint32_t Index2 = (*InOutIndices)[IndexOffset + 2];
		if (Index0 >= InVertices.size() || Index1 >= InVertices.size() || Index2 >= InVertices.size())
		{
			continue;
		}

		const aiVector3D Position0 = ToAiVector(InVertices[Index0].Position);
		const aiVector3D Position1 = ToAiVector(InVertices[Index1].Position);
		const aiVector3D Position2 = ToAiVector(InVertices[Index2].Position);
		const aiVector3D FaceNormal = (Position1 - Position0) ^ (Position2 - Position0);
		if (FaceNormal.SquareLength() <= 1.0e-8f)
		{
			continue;
		}

		aiVector3D VertexNormal =
			ToAiVector(InVertices[Index0].Normal) +
			ToAiVector(InVertices[Index1].Normal) +
			ToAiVector(InVertices[Index2].Normal);
		if (VertexNormal.SquareLength() <= 1.0e-8f)
		{
			continue;
		}

		VertexNormal.Normalize();
		if ((FaceNormal * VertexNormal) < 0.0f)
		{
			std::swap((*InOutIndices)[IndexOffset + 1], (*InOutIndices)[IndexOffset + 2]);
		}
	}
}

aiVector3D TransformPoint(const aiMatrix4x4& InTransform, const aiVector3D& InPoint)
{
	return InTransform * InPoint;
}

aiVector3D TransformNormal(const aiMatrix4x4& InTransform, const aiVector3D& InNormal)
{
	aiMatrix3x3 NormalMatrix(InTransform);
	NormalMatrix.Inverse().Transpose();
	aiVector3D TransformedNormal = NormalMatrix * InNormal;
	if (TransformedNormal.SquareLength() > 1.0e-8f)
	{
		TransformedNormal.Normalize();
	}
	return TransformedNormal;
}

aiVector3D ToUePosition(bool bConvertYUpToZUp, const aiVector3D& InPosition)
{
	return bConvertYUpToZUp ? FAssimpCoordinate::ConvertPositionToUe(InPosition) : InPosition;
}

aiVector3D ToUeDirection(bool bConvertYUpToZUp, const aiVector3D& InDirection)
{
	return bConvertYUpToZUp ? FAssimpCoordinate::ConvertDirectionToUe(InDirection) : InDirection;
}
} // namespace

std::string MeshImporter::GetLastAssimpError()
{
	const char* ErrorText = aiGetErrorString();
	if (ErrorText == nullptr || ErrorText[0] == '\0')
	{
		return "Unknown Assimp import error.";
	}
	return ErrorText;
}

bool MeshImporter::ProbeIsSkeletalModel(const std::filesystem::path& InFilePath)
{
	MeshImporter Importer;
	if (!Importer.LoadScene(InFilePath))
	{
		return false;
	}

	const bool bIsSkeletal = SceneHasSkeletalContent(Importer.Scene_);
	aiReleaseImport(Importer.Scene_);
	Importer.Scene_ = nullptr;
	return bIsSkeletal;
}

bool MeshImporter::LoadScene(const std::filesystem::path& InFilePath)
{
	SetupDefaultImport();

	Scene_ = aiImportFile(
		InFilePath.string().c_str(),
		aiProcess_CalcTangentSpace |
		aiProcess_JoinIdenticalVertices |
		aiProcess_LimitBoneWeights |
		aiProcess_RemoveRedundantMaterials |
		aiProcess_SortByPType |
		aiProcess_Triangulate |
		aiProcess_GenNormals |
		aiProcess_GenUVCoords |
		aiProcess_TransformUVCoords);

	if (Scene_ == nullptr)
	{
		return false;
	}

	CurrentFilePath_ = InFilePath;
	bConvertYUpToZUp_ = FAssimpCoordinate::ShouldConvertYUpToZUp(Scene_);
	return true;
}

UStaticMesh* MeshImporter::ImportStaticMesh(const std::filesystem::path& InFilePath, const std::string& InAssetId)
{
	if (!LoadScene(InFilePath))
	{
		return nullptr;
	}

	if (Scene_->mNumMeshes == 0)
	{
		aiReleaseImport(Scene_);
		return nullptr;
	}

	UStaticMesh* StaticMesh = new UStaticMesh(0, InAssetId, nullptr);
	StaticMesh->SetAssetPath(InAssetId);

	std::unordered_map<unsigned int, aiMatrix4x4> MeshNodeTransforms;
	CollectMeshNodeTransforms(Scene_->mRootNode, aiMatrix4x4(), &MeshNodeTransforms);

	std::vector<UStaticMesh::FVertex> MergedVertices;
	std::vector<uint32_t> MergedIndices;
	for (unsigned int i = 0; i < Scene_->mNumMeshes; ++i)
	{
		aiMesh* AiMesh = Scene_->mMeshes[i];
		const aiMatrix4x4 NodeTransform =
			MeshNodeTransforms.count(i) > 0 ? MeshNodeTransforms[i] : aiMatrix4x4();
		std::unique_ptr<UStaticMesh> SectionMesh(ConvertToStaticMesh(AiMesh, NodeTransform));
		if (SectionMesh != nullptr)
		{
			const uint32_t VertexOffset = static_cast<uint32_t>(MergedVertices.size());
			const uint32_t FirstIndex = static_cast<uint32_t>(MergedIndices.size());
			const std::vector<UStaticMesh::FVertex>& SectionVertices = SectionMesh->GetVertices();
			const std::vector<uint32_t>& SectionIndices = SectionMesh->GetIndices();

			MergedVertices.insert(MergedVertices.end(), SectionVertices.begin(), SectionVertices.end());
			for (const uint32_t SectionIndex : SectionIndices)
			{
				MergedIndices.push_back(VertexOffset + SectionIndex);
			}

			for (size_t j = 0; j < SectionMesh->GetSectionCount(); ++j)
			{
				UStaticMesh::FStaticMeshSection Section = SectionMesh->GetSection(j);
				Section.FirstIndex = FirstIndex + Section.FirstIndex;
				StaticMesh->AddSection(Section);
			}
		}
	}

	StaticMesh->SetVertices(MergedVertices);
	StaticMesh->SetIndices(MergedIndices);
	StaticMesh->RebuildSectionBounds();

	if (Scene_ != nullptr)
	{
		aiReleaseImport(Scene_);
		Scene_ = nullptr;
	}
	return StaticMesh;
}

USkeletalMesh* MeshImporter::ImportSkeletalMesh(const std::filesystem::path& InFilePath, const std::string& InAssetId)
{
	if (!LoadScene(InFilePath))
	{
		return nullptr;
	}

	if (Scene_->mNumMeshes == 0)
	{
		aiReleaseImport(Scene_);
		return nullptr;
	}

	USkeletalMesh* SkeletalMesh = new USkeletalMesh(0, InAssetId, nullptr);
	SkeletalMesh->SetAssetPath(InAssetId);

	std::vector<USkeletalMesh::FBone> Bones;
	std::vector<USkinnedAsset::FSkinVertex> MergedSkinVertices;
	std::vector<uint32_t> MergedIndices;

	if (Scene_->mNumAnimations > 0)
	{
		for (unsigned int i = 0; i < Scene_->mNumAnimations; ++i)
		{
			aiAnimation* AiAnim = Scene_->mAnimations[i];
			(void)AiAnim;
		}
	}

	std::unordered_map<unsigned int, aiMatrix4x4> MeshNodeTransforms;
	CollectMeshNodeTransforms(Scene_->mRootNode, aiMatrix4x4(), &MeshNodeTransforms);

	for (unsigned int i = 0; i < Scene_->mNumMeshes; ++i)
	{
		aiMesh* AiMesh = Scene_->mMeshes[i];

		if (AiMesh == nullptr || AiMesh->mNumVertices == 0)
		{
			continue;
		}

		const aiMatrix4x4 NodeTransform =
			MeshNodeTransforms.count(i) > 0 ? MeshNodeTransforms[i] : aiMatrix4x4();
		std::unique_ptr<USkeletalMesh> SectionMesh(ConvertToSkeletalMesh(AiMesh, NodeTransform));
		if (SectionMesh != nullptr)
		{
			const uint32_t VertexOffset = static_cast<uint32_t>(MergedSkinVertices.size());
			const uint32_t FirstIndex = static_cast<uint32_t>(MergedIndices.size());
			const std::vector<USkinnedAsset::FSkinVertex>& SectionVertices = SectionMesh->GetSkinVertices();
			const std::vector<uint32_t>& SectionIndices = SectionMesh->GetIndices();

			MergedSkinVertices.insert(MergedSkinVertices.end(), SectionVertices.begin(), SectionVertices.end());
			for (const uint32_t SectionIndex : SectionIndices)
			{
				MergedIndices.push_back(VertexOffset + SectionIndex);
			}

			for (size_t j = 0; j < SectionMesh->GetSectionCount(); ++j)
			{
				USkeletalMesh::FSkeletalMeshSection Section = SectionMesh->GetSection(j);
				Section.FirstIndex = FirstIndex + Section.FirstIndex;
				SkeletalMesh->AddSection(Section);
			}
		}
	}

	if (Scene_ != nullptr)
	{
		aiReleaseImport(Scene_);
		Scene_ = nullptr;
	}

	SkeletalMesh->SetSkinVertices(MergedSkinVertices);
	SkeletalMesh->SetIndices(MergedIndices);

	return SkeletalMesh;
}

UStaticMesh* MeshImporter::ConvertToStaticMesh(const aiMesh* InAiMesh, const aiMatrix4x4& InNodeTransform)
{
	if (InAiMesh == nullptr)
	{
		return nullptr;
	}

	UStaticMesh* StaticMesh = new UStaticMesh(0, "", nullptr);

	std::vector<UStaticMesh::FVertex> Vertices;
	Vertices.reserve(InAiMesh->mNumVertices);

	for (unsigned int i = 0; i < InAiMesh->mNumVertices; ++i)
	{
		aiVector3D& Pos = InAiMesh->mVertices[i];
		aiVector3D* Normals = InAiMesh->mNormals;
		aiVector3D* TexCoords = InAiMesh->mTextureCoords[0];
		aiVector3D* Tangents = InAiMesh->mTangents;

		const aiVector3D WorldPosition = TransformPoint(InNodeTransform, Pos);

		UStaticMesh::FVertex Vertex;
		FAssimpCoordinate::AssignFVector3(ToUePosition(bConvertYUpToZUp_, WorldPosition), Vertex.Position);

		if (Normals != nullptr)
		{
			const aiVector3D WorldNormal = TransformNormal(InNodeTransform, Normals[i]);
			FAssimpCoordinate::AssignFVector3(ToUeDirection(bConvertYUpToZUp_, WorldNormal), Vertex.Normal);
		}

		if (TexCoords != nullptr)
		{
			Vertex.TexCoord.X = TexCoords[i].x;
			Vertex.TexCoord.Y = TexCoords[i].y;
		}

		if (Tangents != nullptr)
		{
			const aiVector3D WorldTangent = TransformNormal(InNodeTransform, Tangents[i]);
			FAssimpCoordinate::AssignFVector4(ToUeDirection(bConvertYUpToZUp_, WorldTangent), Vertex.TangentVec);
		}

		Vertices.push_back(Vertex);
	}

	StaticMesh->SetVertices(Vertices);

	std::vector<uint32_t> Indices;
	for (unsigned int i = 0; i < InAiMesh->mNumFaces; ++i)
	{
		aiFace& Face = InAiMesh->mFaces[i];
		for (unsigned int j = 0; j < Face.mNumIndices; ++j)
		{
			Indices.push_back(Face.mIndices[j]);
		}
	}
	NormalizeTriangleWinding(Vertices, &Indices);

	StaticMesh->SetIndices(Indices);

	UStaticMesh::FStaticMeshSection Section;
	Section.MaterialIndex = InAiMesh->mMaterialIndex;
	Section.FirstIndex = 0;
	Section.IndexCount = static_cast<uint32_t>(Indices.size());
	StaticMesh->AddSection(Section);
	StaticMesh->RebuildSectionBounds();

	return StaticMesh;
}

USkeletalMesh* MeshImporter::ConvertToSkeletalMesh(const aiMesh* InAiMesh, const aiMatrix4x4& InNodeTransform)
{
	if (InAiMesh == nullptr)
	{
		return nullptr;
	}

	USkeletalMesh* SkeletalMesh = new USkeletalMesh(0, "", nullptr);

	std::vector<USkeletalMesh::FSkinVertex> Vertices;
	Vertices.reserve(InAiMesh->mNumVertices);

	for (unsigned int i = 0; i < InAiMesh->mNumVertices; ++i)
	{
		aiVector3D& Pos = InAiMesh->mVertices[i];
		aiVector3D* Normals = InAiMesh->mNormals;
		aiVector3D* TexCoords = InAiMesh->mTextureCoords[0];
		aiVector3D* Tangents = InAiMesh->mTangents;

		const aiVector3D WorldPosition = TransformPoint(InNodeTransform, Pos);

		USkeletalMesh::FSkinVertex Vertex;
		FAssimpCoordinate::AssignFVector3(ToUePosition(bConvertYUpToZUp_, WorldPosition), Vertex.Position);

		if (Normals != nullptr)
		{
			const aiVector3D WorldNormal = TransformNormal(InNodeTransform, Normals[i]);
			FAssimpCoordinate::AssignFVector3(ToUeDirection(bConvertYUpToZUp_, WorldNormal), Vertex.Normal);
		}

		if (TexCoords != nullptr)
		{
			Vertex.TexCoord.X = TexCoords[i].x;
			Vertex.TexCoord.Y = TexCoords[i].y;
		}

		Vertex.BoneIndices = { -1, -1, -1, -1 };
		Vertex.BoneWeights = { 0.0f, 0.0f, 0.0f, 0.0f };

		if (Tangents != nullptr)
		{
			const aiVector3D WorldTangent = TransformNormal(InNodeTransform, Tangents[i]);
			FAssimpCoordinate::AssignFVector4(ToUeDirection(bConvertYUpToZUp_, WorldTangent), Vertex.Tangent);
		}

		Vertices.push_back(Vertex);
	}

	SkeletalMesh->SetSkinVertices(Vertices, false);

	std::vector<uint32_t> Indices;
	Indices.reserve(static_cast<size_t>(InAiMesh->mNumFaces) * 3);
	for (unsigned int i = 0; i < InAiMesh->mNumFaces; ++i)
	{
		const aiFace& Face = InAiMesh->mFaces[i];
		if (Face.mNumIndices == 0 || Face.mIndices == nullptr)
		{
			continue;
		}

		for (unsigned int j = 0; j < Face.mNumIndices; ++j)
		{
			const uint32_t VertexIndex = Face.mIndices[j];
			if (VertexIndex >= Vertices.size())
			{
				continue;
			}
			Indices.push_back(VertexIndex);
		}
	}

	NormalizeTriangleWinding(Vertices, &Indices);

	USkeletalMesh::FSkeletalMeshSection Section;
	Section.MaterialIndex = InAiMesh->mMaterialIndex;
	Section.FirstIndex = 0;
	Section.IndexCount = static_cast<uint32_t>(Indices.size());
	SkeletalMesh->AddSection(Section);
	SkeletalMesh->SetIndices(Indices);

	return SkeletalMesh;
}
