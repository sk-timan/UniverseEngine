#include "asset/thumbnail/MeshThumbnailProvider.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <QPainter>
#include <QPointF>

#include "asset/AssetSerializer.h"
#include "asset/AssetTypeInfo.h"
#include "math/FVector3.h"
#include "render/asset/StreamableRenderAsset.h"
#include "render/asset/SkeletalMesh.h"
#include "render/asset/StaticMesh.h"

namespace
{
struct FProjectedTriangle
{
	QPointF Points[3];
	float Depth = 0.0f;
};

QPointF ProjectIsometric(const FVector3& InPosition, float InScale, const QPointF& InCenter)
{
	const float IsoX = (InPosition.X - InPosition.Z) * InScale;
	const float IsoY = (InPosition.Y + (InPosition.X + InPosition.Z) * 0.5f) * InScale;
	return InCenter + QPointF(IsoX, -IsoY);
}

void DrawGridBackground(QPainter& InPainter, int InSize)
{
	InPainter.fillRect(0, 0, InSize, InSize, QColor("#1a1a1e"));

	InPainter.setPen(QPen(QColor("#2e2e34"), 1));
	const int GridStep = 16;
	for (int X = 0; X <= InSize; X += GridStep)
	{
		InPainter.drawLine(X, 0, X, InSize);
	}
	for (int Y = 0; Y <= InSize; Y += GridStep)
	{
		InPainter.drawLine(0, Y, InSize, Y);
	}
}

QImage RenderMeshThumbnail(
	const std::vector<FVector3>& InPositions,
	const std::vector<uint32_t>& InIndices,
	int InSize)
{
	QImage Image(InSize, InSize, QImage::Format_ARGB32);
	Image.fill(Qt::transparent);

	if (InPositions.empty() || InIndices.size() < 3)
	{
		return Image;
	}

	FVector3 Min = InPositions.front();
	FVector3 Max = InPositions.front();
	for (const FVector3& Position : InPositions)
	{
		Min.X = std::min(Min.X, Position.X);
		Min.Y = std::min(Min.Y, Position.Y);
		Min.Z = std::min(Min.Z, Position.Z);
		Max.X = std::max(Max.X, Position.X);
		Max.Y = std::max(Max.Y, Position.Y);
		Max.Z = std::max(Max.Z, Position.Z);
	}

	const FVector3 Center{
		(Min.X + Max.X) * 0.5f,
		(Min.Y + Max.Y) * 0.5f,
		(Min.Z + Max.Z) * 0.5f};
	const float Extent = std::max({Max.X - Min.X, Max.Y - Min.Y, Max.Z - Min.Z, 0.001f});
	const float Scale = (InSize * 0.35f) / Extent;
	const QPointF ScreenCenter(InSize * 0.5, InSize * 0.55);

	std::vector<FProjectedTriangle> Triangles;
	Triangles.reserve(InIndices.size() / 3);
	for (size_t Index = 0; Index + 2 < InIndices.size(); Index += 3)
	{
		const uint32_t I0 = InIndices[Index];
		const uint32_t I1 = InIndices[Index + 1];
		const uint32_t I2 = InIndices[Index + 2];
		if (I0 >= InPositions.size() || I1 >= InPositions.size() || I2 >= InPositions.size())
		{
			continue;
		}

		const FVector3& P0 = InPositions[I0];
		const FVector3& P1 = InPositions[I1];
		const FVector3& P2 = InPositions[I2];
		FProjectedTriangle Triangle;
		Triangle.Points[0] = ProjectIsometric(P0 - Center, Scale, ScreenCenter);
		Triangle.Points[1] = ProjectIsometric(P1 - Center, Scale, ScreenCenter);
		Triangle.Points[2] = ProjectIsometric(P2 - Center, Scale, ScreenCenter);
		Triangle.Depth = (P0.Y + P1.Y + P2.Y) / 3.0f;
		Triangles.push_back(Triangle);
	}

	std::sort(
		Triangles.begin(),
		Triangles.end(),
		[](const FProjectedTriangle& A, const FProjectedTriangle& B)
		{
			return A.Depth < B.Depth;
		});

	QPainter Painter(&Image);
	DrawGridBackground(Painter, InSize);
	Painter.setRenderHint(QPainter::Antialiasing, true);

	for (const FProjectedTriangle& Triangle : Triangles)
	{
		QPolygonF Polygon;
		Polygon << Triangle.Points[0] << Triangle.Points[1] << Triangle.Points[2];
		Painter.setPen(Qt::NoPen);
		Painter.setBrush(QColor("#b8b8c0"));
		Painter.drawPolygon(Polygon);
	}

	Painter.setPen(QPen(QColor("#e8e8f0"), 1));
	Painter.setBrush(Qt::NoBrush);
	for (const FProjectedTriangle& Triangle : Triangles)
	{
		QPolygonF Polygon;
		Polygon << Triangle.Points[0] << Triangle.Points[1] << Triangle.Points[2];
		Painter.drawPolygon(Polygon);
	}

	return Image;
}
} // namespace

bool MeshThumbnailProvider::CanProvide(const std::string& InType) const
{
	return AssetTypeInfo::IsMeshAssetType(InType);
}

QImage MeshThumbnailProvider::Generate(const FAssetRegistryEntry& InEntry, int InSize) const
{
	std::string ErrorMessage;
	UStreamableRenderAsset* LoadedAsset =
		UAssetSerializer::LoadObject(InEntry.UAssetFilePath, &ErrorMessage);
	if (LoadedAsset == nullptr)
	{
		return QImage();
	}

	std::vector<FVector3> Positions;
	std::vector<uint32_t> Indices;

	if (const UStaticMesh* StaticMesh = dynamic_cast<UStaticMesh*>(LoadedAsset))
	{
		for (const UStaticMesh::FVertex& Vertex : StaticMesh->GetVertices())
		{
			Positions.push_back(Vertex.Position);
		}
		Indices = StaticMesh->GetIndices();
	}
	else if (const USkeletalMesh* SkeletalMesh = dynamic_cast<USkeletalMesh*>(LoadedAsset))
	{
		for (const USkinnedAsset::FSkinVertex& Vertex : SkeletalMesh->GetSkinVertices())
		{
			Positions.push_back(Vertex.Position);
		}
		Indices = SkeletalMesh->GetIndices();
	}

	const QImage Thumbnail = RenderMeshThumbnail(Positions, Indices, InSize);
	delete LoadedAsset;
	if (Positions.empty() || Indices.size() < 3)
	{
		return QImage();
	}
	return Thumbnail;
}
