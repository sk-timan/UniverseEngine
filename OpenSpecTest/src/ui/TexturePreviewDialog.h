#pragma once

#include <QDialog>

#include "asset/AssetRegistry.h"

class QLabel;

class TexturePreviewDialog final : public QDialog
{
	Q_OBJECT

public:
	explicit TexturePreviewDialog(QWidget* InParent = nullptr);

	bool LoadFromRegistryEntry(const FAssetRegistryEntry& InEntry);

private:
	void BuildUi();

	QLabel* m_preview_label_ = nullptr;
	QLabel* m_info_label_ = nullptr;
};
