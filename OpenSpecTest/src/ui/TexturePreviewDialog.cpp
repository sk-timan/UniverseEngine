#include "ui/TexturePreviewDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include "asset/AssetSerializer.h"
#include "asset/SoftObjectPath.h"
#include "render/asset/StreamableRenderAsset.h"
#include "render/asset/Texture2D.h"

TexturePreviewDialog::TexturePreviewDialog(QWidget* InParent)
	: QDialog(InParent)
{
	setWindowTitle(tr("纹理预览"));
	setModal(true);
	BuildUi();
	resize(520, 560);
}

void TexturePreviewDialog::BuildUi()
{
	auto* RootLayout = new QVBoxLayout(this);

	m_preview_label_ = new QLabel(this);
	m_preview_label_->setAlignment(Qt::AlignCenter);
	m_preview_label_->setMinimumSize(256, 256);
	m_preview_label_->setStyleSheet(QStringLiteral("background-color: #1a1a1e; border: 1px solid #333;"));
	m_preview_label_->setScaledContents(false);
	RootLayout->addWidget(m_preview_label_, 1);

	m_info_label_ = new QLabel(this);
	m_info_label_->setWordWrap(true);
	m_info_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
	RootLayout->addWidget(m_info_label_);

	auto* ButtonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(ButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(ButtonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &QDialog::accept);
	RootLayout->addWidget(ButtonBox);
}

bool TexturePreviewDialog::LoadFromRegistryEntry(const FAssetRegistryEntry& InEntry)
{
	std::string ErrorMessage;
	UStreamableRenderAsset* LoadedAsset =
		UAssetSerializer::LoadObject(InEntry.UAssetFilePath, &ErrorMessage);
	if (LoadedAsset == nullptr)
	{
		m_info_label_->setText(tr("无法加载资产: %1").arg(QString::fromStdString(ErrorMessage)));
		return false;
	}

	const UTexture2D* Texture2D = dynamic_cast<UTexture2D*>(LoadedAsset);
	if (Texture2D == nullptr || !Texture2D->HasResidentPlatformData())
	{
		delete LoadedAsset;
		m_info_label_->setText(tr("资产不是有效的 Texture2D。"));
		return false;
	}

	const FTextureMipLevel* Mip0 = Texture2D->GetPlatformData().GetMip(0);
	if (Mip0 == nullptr || Mip0->Data.empty() || Mip0->Width == 0 || Mip0->Height == 0)
	{
		delete LoadedAsset;
		m_info_label_->setText(tr("纹理缺少 mip0 像素数据。"));
		return false;
	}

	QImage SourceImage(
		reinterpret_cast<const uchar*>(Mip0->Data.data()),
		static_cast<int>(Mip0->Width),
		static_cast<int>(Mip0->Height),
		static_cast<int>(Mip0->Width) * 4,
		QImage::Format_RGBA8888);

	const QPixmap PreviewPixmap = QPixmap::fromImage(SourceImage.copy());
	const int MaxPreviewSize = 480;
	m_preview_label_->setPixmap(
		PreviewPixmap.scaled(
			MaxPreviewSize,
			MaxPreviewSize,
			Qt::KeepAspectRatio,
			Qt::SmoothTransformation));

	const QString SoftPath = QString::fromStdString(
		FSoftObjectPath::Build(InEntry.AssetPath, InEntry.ObjectName));
	const QString SourceFile =
		InEntry.SourceFile.empty() ? tr("（无）") : QString::fromStdString(InEntry.SourceFile);

	m_info_label_->setText(
		tr("SoftObjectPath: %1\n类型: %2\n尺寸: %3 × %4\nSourceFile: %5")
			.arg(SoftPath)
			.arg(QString::fromStdString(InEntry.Type))
			.arg(Mip0->Width)
			.arg(Mip0->Height)
			.arg(SourceFile));

	delete LoadedAsset;
	return true;
}
