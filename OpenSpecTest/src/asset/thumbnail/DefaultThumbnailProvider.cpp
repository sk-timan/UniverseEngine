#include "asset/thumbnail/DefaultThumbnailProvider.h"

#include <QPainter>

bool DefaultThumbnailProvider::CanProvide(const std::string& InType) const
{
	(void)InType;
	return true;
}

QImage DefaultThumbnailProvider::Generate(const FAssetRegistryEntry& InEntry, int InSize) const
{
	(void)InEntry;

	QImage Image(InSize, InSize, QImage::Format_ARGB32);
	Image.fill(QColor("#2a2a2e"));

	QPainter Painter(&Image);
	Painter.setRenderHint(QPainter::Antialiasing, true);

	const int Margin = InSize / 6;
	const QRect DocRect(Margin, Margin / 2, InSize - Margin * 2, InSize - Margin);
	Painter.setPen(QPen(QColor("#6a6a70"), 2));
	Painter.setBrush(QColor("#3a3a3f"));
	Painter.drawRoundedRect(DocRect, 4, 4);

	Painter.setPen(QPen(QColor("#808088"), 1));
	for (int Line = 0; Line < 3; ++Line)
	{
		const int Y = DocRect.top() + DocRect.height() / 4 + Line * (DocRect.height() / 5);
		Painter.drawLine(DocRect.left() + 8, Y, DocRect.right() - 8, Y);
	}

	return Image;
}
