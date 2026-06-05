#include "ui/EditorPerformanceDialog.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QVBoxLayout>

EditorPerformanceDialog::EditorPerformanceDialog(
	const FEditorPerformanceSettings& InInitialSettings,
	QWidget* InParent)
	: QDialog(InParent)
{
	setWindowTitle(tr("性能"));
	setModal(true);
	BuildUi();

	if (InInitialSettings.TriangleBvhSplitMethod == EPickTriangleBvhSplitMethod::Sah)
	{
		if (m_sah_split_radio_ != nullptr)
		{
			m_sah_split_radio_->setChecked(true);
		}
	}
	else if (m_median_split_radio_ != nullptr)
	{
		m_median_split_radio_->setChecked(true);
	}

	resize(420, 180);
}

void EditorPerformanceDialog::BuildUi()
{
	auto* RootLayout = new QVBoxLayout(this);

	auto* PickingGroup = new QGroupBox(tr("视口拾取"), this);
	auto* PickingLayout = new QFormLayout(PickingGroup);

	m_median_split_radio_ = new QRadioButton(tr("Median Split（默认）"), PickingGroup);
	m_sah_split_radio_ = new QRadioButton(tr("SAH Split"), PickingGroup);
	m_median_split_radio_->setChecked(true);

	m_split_method_group_ = new QButtonGroup(PickingGroup);
	m_split_method_group_->addButton(m_median_split_radio_, static_cast<int>(EPickTriangleBvhSplitMethod::Median));
	m_split_method_group_->addButton(m_sah_split_radio_, static_cast<int>(EPickTriangleBvhSplitMethod::Sah));

	auto* SplitMethodRow = new QWidget(PickingGroup);
	auto* SplitMethodLayout = new QVBoxLayout(SplitMethodRow);
	SplitMethodLayout->setContentsMargins(0, 0, 0, 0);
	SplitMethodLayout->addWidget(m_median_split_radio_);
	SplitMethodLayout->addWidget(m_sah_split_radio_);
	PickingLayout->addRow(tr("三角面 BVH 分割"), SplitMethodRow);

	RootLayout->addWidget(PickingGroup);

	auto* ButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(ButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(ButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	RootLayout->addWidget(ButtonBox);
}

FEditorPerformanceSettings EditorPerformanceDialog::GetSettings() const
{
	FEditorPerformanceSettings Result;
	if (m_split_method_group_ != nullptr
		&& m_split_method_group_->checkedId() == static_cast<int>(EPickTriangleBvhSplitMethod::Sah))
	{
		Result.TriangleBvhSplitMethod = EPickTriangleBvhSplitMethod::Sah;
	}
	return Result;
}
