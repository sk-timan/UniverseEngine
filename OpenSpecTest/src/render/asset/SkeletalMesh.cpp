#include "render/asset/SkeletalMesh.h"

#include <utility>

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
