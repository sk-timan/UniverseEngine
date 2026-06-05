#include "render/asset/StaticMesh.h"

#include <cmath>
#include <utility>

#include <nlohmann/json.hpp>

#include "core/ObjectRegistry.h"

namespace
{
std::unique_ptr<UStaticMesh> CreateStaticMeshInstance(uint64_t InObjectId, std::string InObjectName)
{
	return std::make_unique<UStaticMesh>(InObjectId, std::move(InObjectName), &UStaticMesh::StaticClass());
}
} // namespace

UStaticMesh::UStaticMesh(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
	: UStreamableRenderAsset(InObjectId, std::move(InObjectName), InClass != nullptr ? InClass : &UStaticMesh::StaticClass())
{
}

const UClass& UStaticMesh::StaticClass()
{
	static const UClass Class("UStaticMesh", &UStreamableRenderAsset::StaticClass(), CreateStaticMeshInstance);
	return Class;
}

void UStaticMesh::SetVertices(const std::vector<FVertex>& InVertices)
{
	Vertices_ = InVertices;

	TotalBounds_.Origin = {0.0f, 0.0f, 0.0f};
	TotalBounds_.Extent = {0.0f, 0.0f, 0.0f};
	TotalBounds_.SphereRadius = 0.0f;

	if (Vertices_.empty())
	{
		return;
	}

	float MinX = Vertices_[0].Position.X;
	float MaxX = Vertices_[0].Position.X;
	float MinY = Vertices_[0].Position.Y;
	float MaxY = Vertices_[0].Position.Y;
	float MinZ = Vertices_[0].Position.Z;
	float MaxZ = Vertices_[0].Position.Z;

	for (const auto& Vertex : Vertices_)
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

	float HalfWidth = TotalBounds_.Extent.X;
	float HalfHeight = TotalBounds_.Extent.Y;
	float HalfDepth = TotalBounds_.Extent.Z;
	TotalBounds_.SphereRadius = std::sqrt(HalfWidth * HalfWidth + HalfHeight * HalfHeight + HalfDepth * HalfDepth);
}

const std::vector<UStaticMesh::FVertex>& UStaticMesh::GetVertices() const
{
	return Vertices_;
}

void UStaticMesh::SetIndices(const std::vector<uint32_t>& InIndices)
{
	Indices_ = InIndices;
}

const std::vector<uint32_t>& UStaticMesh::GetIndices() const
{
	return Indices_;
}

void UStaticMesh::AddSection(const FStaticMeshSection& InSection)
{
	Sections_.push_back(InSection);
}

size_t UStaticMesh::GetSectionCount() const
{
	return Sections_.size();
}

const UStaticMesh::FStaticMeshSection& UStaticMesh::GetSection(size_t InIndex) const
{
	return Sections_[InIndex];
}

void UStaticMesh::RebuildSectionBounds()
{
	for (FStaticMeshSection& Section : Sections_)
	{
		Section.SectionBounds = {};
		if (Section.IndexCount == 0 || Indices_.empty() || Vertices_.empty())
		{
			continue;
		}

		const uint32_t EndIndex = Section.FirstIndex + Section.IndexCount;
		if (EndIndex > static_cast<uint32_t>(Indices_.size()))
		{
			continue;
		}

		bool bHasPoint = false;
		float MinX = 0.0f;
		float MaxX = 0.0f;
		float MinY = 0.0f;
		float MaxY = 0.0f;
		float MinZ = 0.0f;
		float MaxZ = 0.0f;

		for (uint32_t IndexOffset = Section.FirstIndex; IndexOffset < EndIndex; ++IndexOffset)
		{
			const uint32_t VertexIndex = Indices_[IndexOffset];
			if (VertexIndex >= static_cast<uint32_t>(Vertices_.size()))
			{
				continue;
			}

			const FVector3& Position = Vertices_[VertexIndex].Position;
			if (!bHasPoint)
			{
				MinX = MaxX = Position.X;
				MinY = MaxY = Position.Y;
				MinZ = MaxZ = Position.Z;
				bHasPoint = true;
			}
			else
			{
				MinX = std::min(MinX, Position.X);
				MaxX = std::max(MaxX, Position.X);
				MinY = std::min(MinY, Position.Y);
				MaxY = std::max(MaxY, Position.Y);
				MinZ = std::min(MinZ, Position.Z);
				MaxZ = std::max(MaxZ, Position.Z);
			}
		}

		if (!bHasPoint)
		{
			continue;
		}

		Section.SectionBounds.Origin.X = (MinX + MaxX) * 0.5f;
		Section.SectionBounds.Origin.Y = (MinY + MaxY) * 0.5f;
		Section.SectionBounds.Origin.Z = (MinZ + MaxZ) * 0.5f;
		Section.SectionBounds.Extent.X = (MaxX - MinX) * 0.5f;
		Section.SectionBounds.Extent.Y = (MaxY - MinY) * 0.5f;
		Section.SectionBounds.Extent.Z = (MaxZ - MinZ) * 0.5f;

		const float HalfWidth = Section.SectionBounds.Extent.X;
		const float HalfHeight = Section.SectionBounds.Extent.Y;
		const float HalfDepth = Section.SectionBounds.Extent.Z;
		Section.SectionBounds.SphereRadius = std::sqrt(
			HalfWidth * HalfWidth + HalfHeight * HalfHeight + HalfDepth * HalfDepth);
	}
}

bool UStaticMesh::HasResidentGeometryData() const
{
	return !Vertices_.empty() && !Indices_.empty() && !Sections_.empty();
}

UStreamableRenderAsset::FBounds UStaticMesh::GetBounds() const
{
	return TotalBounds_;
}

void UStaticMesh::Serialize(nlohmann::json* OutObjectJson) const
{
	UStreamableRenderAsset::Serialize(OutObjectJson);
	if (OutObjectJson == nullptr)
	{
		return;
	}

	(*OutObjectJson)["vertex_count"] = Vertices_.size();
	(*OutObjectJson)["index_count"] = Indices_.size();
	(*OutObjectJson)["section_count"] = Sections_.size();
}
