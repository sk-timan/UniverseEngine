#include "render/asset/SkeletalMesh.h"

#include <utility>

#include <nlohmann/json.hpp>

#include "core/ObjectRegistry.h"

namespace
{
std::unique_ptr<USkeletalMesh> CreateSkeletalMeshInstance(uint64_t InObjectId, std::string InObjectName)
{
	return std::make_unique<USkeletalMesh>(InObjectId, std::move(InObjectName), &USkeletalMesh::StaticClass());
}
} // namespace

USkeletalMesh::USkeletalMesh(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
	: USkinnedAsset(InObjectId, std::move(InObjectName), InClass != nullptr ? InClass : &USkeletalMesh::StaticClass())
{
}

const UClass& USkeletalMesh::StaticClass()
{
	static const UClass Class("USkeletalMesh", &USkinnedAsset::StaticClass(), CreateSkeletalMeshInstance);
	return Class;
}

void USkeletalMesh::AddSection(const FSkeletalMeshSection& InSection)
{
	Sections_.push_back(InSection);
}

void USkeletalMesh::SetIndices(const std::vector<uint32_t>& InIndices)
{
	Indices_ = InIndices;
}

const std::vector<uint32_t>& USkeletalMesh::GetIndices() const
{
	return Indices_;
}

size_t USkeletalMesh::GetSectionCount() const
{
	return Sections_.size();
}

const USkeletalMesh::FSkeletalMeshSection& USkeletalMesh::GetSection(size_t InIndex) const
{
	return Sections_[InIndex];
}

bool USkeletalMesh::HasResidentGeometryData() const
{
	return USkinnedAsset::HasResidentGeometryData() && !Sections_.empty() && !Indices_.empty();
}

void USkeletalMesh::Serialize(nlohmann::json* OutObjectJson) const
{
	USkinnedAsset::Serialize(OutObjectJson);
	if (OutObjectJson == nullptr)
	{
		return;
	}

	(*OutObjectJson)["class"] = "USkeletalMesh";
	(*OutObjectJson)["indices"] = Indices_;

	nlohmann::json SectionsJson = nlohmann::json::array();
	for (const FSkeletalMeshSection& Section : Sections_)
	{
		SectionsJson.push_back(nlohmann::json{
			{"material_index", Section.MaterialIndex},
			{"first_index", Section.FirstIndex},
			{"index_count", Section.IndexCount},
			{"bone_indices", Section.BoneIndices}
		});
	}
	(*OutObjectJson)["sections"] = std::move(SectionsJson);
}

USkeletalMesh* USkeletalMesh::Deserialize(const nlohmann::json& InObjectJson, std::string* OutErrorMessage)
{
	USkinnedAsset* BaseAsset = USkinnedAsset::DeserializeBase(InObjectJson, OutErrorMessage);
	if (BaseAsset == nullptr)
	{
		return nullptr;
	}

	USkeletalMesh* SkeletalMesh = new USkeletalMesh(0, BaseAsset->GetObjectName(), nullptr);
	SkeletalMesh->SetAssetPath(BaseAsset->GetAssetPath());
	SkeletalMesh->SetSkeleton(BaseAsset->GetSkeleton());
	SkeletalMesh->SetSkinVertices(BaseAsset->GetSkinVertices());
	delete BaseAsset;

	if (InObjectJson.contains("indices"))
	{
		SkeletalMesh->SetIndices(InObjectJson.at("indices").get<std::vector<uint32_t>>());
	}
	if (InObjectJson.contains("sections"))
	{
		for (const nlohmann::json& SectionJson : InObjectJson.at("sections"))
		{
			FSkeletalMeshSection Section;
			Section.MaterialIndex = SectionJson.at("material_index").get<uint32_t>();
			Section.FirstIndex = SectionJson.at("first_index").get<uint32_t>();
			Section.IndexCount = SectionJson.at("index_count").get<uint32_t>();
			if (SectionJson.contains("bone_indices"))
			{
				Section.BoneIndices = SectionJson.at("bone_indices").get<std::vector<int32_t>>();
			}
			SkeletalMesh->AddSection(Section);
		}
	}

	return SkeletalMesh;
}
