#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/UClass.h"
#include "render/asset/StreamableRenderAsset.h"

#include "math/FVector3.h"
#include "math/FVector2D.h"
#include "math/FVector4.h"

class UStaticMesh : public UStreamableRenderAsset
{
public:
	struct FVertex
	{
		FVector3 Position;
		FVector3 Normal;
		FVector2D TexCoord;
		FVector4 TangentVec;
	};

	struct FStaticMeshSection
	{
		uint32_t MaterialIndex = 0;
		uint32_t FirstIndex = 0;
		uint32_t IndexCount = 0;
		FBounds SectionBounds;
		void* VertexBufferGPU = nullptr;
		void* IndexBufferGPU = nullptr;
	};

	UStaticMesh(uint64_t InObjectId, std::string InObjectName, const UClass* InClass = nullptr);
	virtual ~UStaticMesh() = default;

	static const UClass& StaticClass();

	void SetVertices(const std::vector<FVertex>& InVertices);
	const std::vector<FVertex>& GetVertices() const;

	void SetIndices(const std::vector<uint32_t>& InIndices);
	const std::vector<uint32_t>& GetIndices() const;

	void AddSection(const FStaticMeshSection& InSection);
	size_t GetSectionCount() const;
	const FStaticMeshSection& GetSection(size_t InIndex) const;
	void RebuildSectionBounds();

	virtual bool HasResidentGeometryData() const override;
	virtual FBounds GetBounds() const override;

	virtual void Serialize(nlohmann::json* OutObjectJson) const override;

private:
	std::vector<FVertex> Vertices_;
	std::vector<uint32_t> Indices_;
	std::vector<FStaticMeshSection> Sections_;
	FBounds TotalBounds_;
};
