#include "asset/thumbnail/TextureThumbnailProvider.h"

#include <QPainter>

#include "asset/AssetSerializer.h"
#include "asset/AssetTypeInfo.h"
#include "render/asset/StreamableRenderAsset.h"
#include "render/asset/Texture2D.h"

namespace
{
QImage RenderTextureThumbnail(const FTextureMipLevel& InMip0, int InSize)
{
	if (InMip0.Width == 0 || InMip0.Height == 0 || InMip0.Data.empty())
	{
		return QImage();
	}

	QImage SourceImage(
		reinterpret_cast<const uchar*>(InMip0.Data.data()),
		static_cast<int>(InMip0.Width),
		static_cast<int>(InMip0.Height),
		static_cast<int>(InMip0.Width) * 4,
		QImage::Format_RGBA8888);

	QImage Thumbnail(InSize, InSize, QImage::Format_RGBA8888);
	Thumbnail.fill(QColor("#1a1a1e"));

	QPainter Painter(&Thumbnail);
	Painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
	Painter.drawImage(QRect(0, 0, InSize, InSize), SourceImage);
	return Thumbnail;
}
} // namespace

bool TextureThumbnailProvider::CanProvide(const std::string& InType) const
{
	return AssetTypeInfo::IsTextureAssetType(InType);
}

QImage TextureThumbnailProvider::Generate(const FAssetRegistryEntry& InEntry, int InSize) const
{
	std::string ErrorMessage;
	UStreamableRenderAsset* LoadedAsset =
		UAssetSerializer::LoadObject(InEntry.UAssetFilePath, &ErrorMessage);
	if (LoadedAsset == nullptr)
	{
		return QImage();
	}

	const UTexture2D* Texture2D = dynamic_cast<UTexture2D*>(LoadedAsset);
	if (Texture2D == nullptr || !Texture2D->HasResidentPlatformData())
	{
		delete LoadedAsset;
		return QImage();
	}

	const FTextureMipLevel* Mip0 = Texture2D->GetPlatformData().GetMip(0);
	if (Mip0 == nullptr)
	{
		delete LoadedAsset;
		return QImage();
	}

	const QImage Thumbnail = RenderTextureThumbnail(*Mip0, InSize);
	delete LoadedAsset;
	return Thumbnail;
}
