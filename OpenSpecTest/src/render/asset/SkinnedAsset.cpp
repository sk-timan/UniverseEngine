#include "render/asset/SkinnedAsset.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <nlohmann/json.hpp>

#include "core/ObjectRegistry.h"

namespace
{
std::unique_ptr<USkinnedAsset> CreateSkinnedAssetInstance(uint64_t InObjectId, std::string InObjectName)
{
	return std::make_unique<USkinnedAsset>(InObjectId, std::move(InObjectName), &USkinnedAsset::StaticClass());
}
} // namespace

USkinnedAsset::USkinnedAsset(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
	: UStreamableRenderAsset(InObjectId, std::move(InObjectName), InClass != nullptr ? InClass : &USkinnedAsset::StaticClass())
{
}

const UClass& USkinnedAsset::StaticClass()
{
	static const UClass Class("USkinnedAsset", &UStreamableRenderAsset::StaticClass(), CreateSkinnedAssetInstance);
	return Class;
}

void USkinnedAsset::SetSkeleton(const std::vector<FBone>& InBones)
{
	Skeleton_ = InBones;
}

const std::vector<USkinnedAsset::FBone>& USkinnedAsset::GetSkeleton() const
{
	return Skeleton_;
}

int32_t USkinnedAsset::FindBoneIndex(const std::string& InBoneName) const
{
	for (size_t i = 0; i < Skeleton_.size(); ++i)
	{
		if (Skeleton_[i].Name == InBoneName)
		{
			return static_cast<int32_t>(i);
		}
	}
	return -1;
}

void USkinnedAsset::SetSkinVertices(const std::vector<FSkinVertex>& InVertices)
{
	SetSkinVertices(InVertices, true);
}

void USkinnedAsset::SetSkinVertices(const std::vector<FSkinVertex>& InVertices, bool bUpdateBounds)
{
	SkinVertices_ = InVertices;
	if (bUpdateBounds)
	{
		RebuildBoundsFromSkinVertices();
	}
}

void USkinnedAsset::RebuildBoundsFromSkinVertices()
{
	TotalBounds_.Origin = {0.0f, 0.0f, 0.0f};
	TotalBounds_.Extent = {0.0f, 0.0f, 0.0f};
	TotalBounds_.SphereRadius = 0.0f;

	if (SkinVertices_.empty())
	{
		return;
	}

	float MinX = SkinVertices_[0].Position.X;
	float MaxX = SkinVertices_[0].Position.X;
	float MinY = SkinVertices_[0].Position.Y;
	float MaxY = SkinVertices_[0].Position.Y;
	float MinZ = SkinVertices_[0].Position.Z;
	float MaxZ = SkinVertices_[0].Position.Z;

	for (const FSkinVertex& Vertex : SkinVertices_)
	{
		MinX = std::min(MinX, Vertex.Position.X);
		MaxX = std::max(MaxX, Vertex.Position.X);
		MinY = std::min(MinY, Vertex.Position.Y);
		MaxY = std::max(MaxY, Vertex.Position.Y);
		MinZ = std::min(MinZ, Vertex.Position.Z);
		MaxZ = std::max(MaxZ, Vertex.Position.Z);
	}

	TotalBounds_.Origin.X = (MinX + MaxX) * 0.5f;
	TotalBounds_.Origin.Y = (MinY + MaxY) * 0.5f;
	TotalBounds_.Origin.Z = (MinZ + MaxZ) * 0.5f;
	TotalBounds_.Extent.X = (MaxX - MinX) * 0.5f;
	TotalBounds_.Extent.Y = (MaxY - MinY) * 0.5f;
	TotalBounds_.Extent.Z = (MaxZ - MinZ) * 0.5f;

	const float HalfWidth = TotalBounds_.Extent.X;
	const float HalfHeight = TotalBounds_.Extent.Y;
	const float HalfDepth = TotalBounds_.Extent.Z;
	TotalBounds_.SphereRadius =
		std::sqrt(HalfWidth * HalfWidth + HalfHeight * HalfHeight + HalfDepth * HalfDepth);
}

UStreamableRenderAsset::FBounds USkinnedAsset::GetBounds() const
{
	return TotalBounds_;
}

const std::vector<USkinnedAsset::FSkinVertex>& USkinnedAsset::GetSkinVertices() const
{
	return SkinVertices_;
}

bool USkinnedAsset::HasResidentGeometryData() const
{
	return !SkinVertices_.empty();
}

void USkinnedAsset::Serialize(nlohmann::json* OutObjectJson) const
{
	UStreamableRenderAsset::Serialize(OutObjectJson);
	if (OutObjectJson == nullptr)
	{
		return;
	}

	nlohmann::json BonesJson = nlohmann::json::array();
	for (const FBone& Bone : Skeleton_)
	{
		BonesJson.push_back(nlohmann::json{
			{"name", Bone.Name},
			{"parent_index", Bone.ParentIndex},
			{"reference_pose", nlohmann::json{
				{"translation", nlohmann::json{{"x", Bone.ReferencePose.Translation.X}, {"y", Bone.ReferencePose.Translation.Y}, {"z", Bone.ReferencePose.Translation.Z}}},
				{"rotation", nlohmann::json{{"pitch", Bone.ReferencePose.Rotation.Pitch}, {"yaw", Bone.ReferencePose.Rotation.Yaw}, {"roll", Bone.ReferencePose.Rotation.Roll}}},
				{"scale", nlohmann::json{{"x", Bone.ReferencePose.Scale.X}, {"y", Bone.ReferencePose.Scale.Y}, {"z", Bone.ReferencePose.Scale.Z}}}
			}}
		});
	}

	nlohmann::json VerticesJson = nlohmann::json::array();
	for (const FSkinVertex& Vertex : SkinVertices_)
	{
		VerticesJson.push_back(nlohmann::json{
			{"position", nlohmann::json{{"x", Vertex.Position.X}, {"y", Vertex.Position.Y}, {"z", Vertex.Position.Z}}},
			{"normal", nlohmann::json{{"x", Vertex.Normal.X}, {"y", Vertex.Normal.Y}, {"z", Vertex.Normal.Z}}},
			{"tex_coord", nlohmann::json{{"x", Vertex.TexCoord.X}, {"y", Vertex.TexCoord.Y}}},
			{"tangent", nlohmann::json{{"x", Vertex.Tangent.X}, {"y", Vertex.Tangent.Y}, {"z", Vertex.Tangent.Z}, {"w", Vertex.Tangent.W}}},
			{"bone_indices", std::vector<int32_t>(Vertex.BoneIndices.begin(), Vertex.BoneIndices.end())},
			{"bone_weights", std::vector<float>(Vertex.BoneWeights.begin(), Vertex.BoneWeights.end())}
		});
	}

	(*OutObjectJson)["class"] = "USkinnedAsset";
	(*OutObjectJson)["skeleton"] = std::move(BonesJson);
	(*OutObjectJson)["skin_vertices"] = std::move(VerticesJson);
	(*OutObjectJson)["total_bounds"] = nlohmann::json{
		{"origin", nlohmann::json{{"x", TotalBounds_.Origin.X}, {"y", TotalBounds_.Origin.Y}, {"z", TotalBounds_.Origin.Z}}},
		{"extent", nlohmann::json{{"x", TotalBounds_.Extent.X}, {"y", TotalBounds_.Extent.Y}, {"z", TotalBounds_.Extent.Z}}},
		{"sphere_radius", TotalBounds_.SphereRadius}
	};
}

USkinnedAsset* USkinnedAsset::DeserializeBase(const nlohmann::json& InObjectJson, std::string* OutErrorMessage)
{
	(void)OutErrorMessage;
	std::string ObjectName = InObjectJson.value("object_name", std::string("SkinnedAsset"));
	USkinnedAsset* Asset = new USkinnedAsset(0, ObjectName, nullptr);
	if (InObjectJson.contains("asset_path"))
	{
		Asset->SetAssetPath(InObjectJson.at("asset_path").get<std::string>());
	}

	if (InObjectJson.contains("skeleton"))
	{
		std::vector<FBone> Bones;
		for (const nlohmann::json& BoneJson : InObjectJson.at("skeleton"))
		{
			FBone Bone;
			Bone.Name = BoneJson.at("name").get<std::string>();
			Bone.ParentIndex = BoneJson.at("parent_index").get<int32_t>();
			if (BoneJson.contains("reference_pose"))
			{
				const auto& PoseJson = BoneJson.at("reference_pose");
				const auto& Translation = PoseJson.at("translation");
				Bone.ReferencePose.Translation.X = Translation.at("x").get<float>();
				Bone.ReferencePose.Translation.Y = Translation.at("y").get<float>();
				Bone.ReferencePose.Translation.Z = Translation.at("z").get<float>();
				const auto& Rotation = PoseJson.at("rotation");
				Bone.ReferencePose.Rotation.Pitch = Rotation.at("pitch").get<float>();
				Bone.ReferencePose.Rotation.Yaw = Rotation.at("yaw").get<float>();
				Bone.ReferencePose.Rotation.Roll = Rotation.at("roll").get<float>();
				const auto& Scale = PoseJson.at("scale");
				Bone.ReferencePose.Scale.X = Scale.at("x").get<float>();
				Bone.ReferencePose.Scale.Y = Scale.at("y").get<float>();
				Bone.ReferencePose.Scale.Z = Scale.at("z").get<float>();
			}
			Bones.push_back(Bone);
		}
		Asset->SetSkeleton(Bones);
	}

	if (InObjectJson.contains("skin_vertices"))
	{
		std::vector<FSkinVertex> Vertices;
		for (const nlohmann::json& VertexJson : InObjectJson.at("skin_vertices"))
		{
			FSkinVertex Vertex;
			const auto& Position = VertexJson.at("position");
			Vertex.Position.X = Position.at("x").get<float>();
			Vertex.Position.Y = Position.at("y").get<float>();
			Vertex.Position.Z = Position.at("z").get<float>();
			const auto& Normal = VertexJson.at("normal");
			Vertex.Normal.X = Normal.at("x").get<float>();
			Vertex.Normal.Y = Normal.at("y").get<float>();
			Vertex.Normal.Z = Normal.at("z").get<float>();
			const auto& TexCoord = VertexJson.at("tex_coord");
			Vertex.TexCoord.X = TexCoord.at("x").get<float>();
			Vertex.TexCoord.Y = TexCoord.at("y").get<float>();
			const auto& Tangent = VertexJson.at("tangent");
			Vertex.Tangent.X = Tangent.at("x").get<float>();
			Vertex.Tangent.Y = Tangent.at("y").get<float>();
			Vertex.Tangent.Z = Tangent.at("z").get<float>();
			Vertex.Tangent.W = Tangent.at("w").get<float>();
			const auto BoneIndices = VertexJson.at("bone_indices").get<std::vector<int32_t>>();
			const auto BoneWeights = VertexJson.at("bone_weights").get<std::vector<float>>();
			for (size_t Index = 0; Index < Vertex.BoneIndices.size() && Index < BoneIndices.size(); ++Index)
			{
				Vertex.BoneIndices[Index] = BoneIndices[Index];
			}
			for (size_t Index = 0; Index < Vertex.BoneWeights.size() && Index < BoneWeights.size(); ++Index)
			{
				Vertex.BoneWeights[Index] = BoneWeights[Index];
			}
			Vertices.push_back(Vertex);
		}
		Asset->SetSkinVertices(Vertices);
	}

	return Asset;
}
