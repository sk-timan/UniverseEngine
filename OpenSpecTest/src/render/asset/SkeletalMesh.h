#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "core/UClass.h"
#include "render/asset/SkinnedAsset.h"

class USkeletalMesh : public USkinnedAsset
{
public:
	struct FSkeletalMeshSection
	{
		uint32_t MaterialIndex = 0;
		uint32_t FirstIndex = 0;
		uint32_t IndexCount = 0;
		std::vector<int32_t> BoneIndices;
		void* VertexBufferGPU = nullptr;
		void* IndexBufferGPU = nullptr;
	};

	USkeletalMesh(uint64_t InObjectId, std::string InObjectName, const UClass* InClass = nullptr);
	virtual ~USkeletalMesh() = default;

	static const UClass& StaticClass();

	void AddSection(const FSkeletalMeshSection& InSection);
	size_t GetSectionCount() const;
	const FSkeletalMeshSection& GetSection(size_t InIndex) const;

	void SetIndices(const std::vector<uint32_t>& InIndices);
	const std::vector<uint32_t>& GetIndices() const;

	virtual bool HasResidentGeometryData() const override;

	virtual void Serialize(nlohmann::json* OutObjectJson) const override;
	static USkeletalMesh* Deserialize(const nlohmann::json& InObjectJson, std::string* OutErrorMessage);

private:
	std::vector<uint32_t> Indices_;
	std::vector<FSkeletalMeshSection> Sections_;
};
