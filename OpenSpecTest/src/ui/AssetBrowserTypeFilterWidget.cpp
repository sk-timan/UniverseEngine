#include "ui/AssetBrowserTypeFilterWidget.h"

#include <QAction>
#include <QHBoxLayout>
#include <QMenu>
#include <QSignalBlocker>
#include <QToolButton>

namespace
{
constexpr const char* kTypeFilterOther = "__other__";
}

AssetBrowserTypeFilterWidget::AssetBrowserTypeFilterWidget(QWidget* InParent)
	: QWidget(InParent)
{
	auto* RootLayout = new QHBoxLayout(this);
	RootLayout->setContentsMargins(0, 0, 0, 0);
	RootLayout->setSpacing(0);

	m_button = new QToolButton(this);
	m_button->setObjectName("AssetBrowserTypeFilterButton");
	m_button->setPopupMode(QToolButton::InstantPopup);
	m_button->setToolButtonStyle(Qt::ToolButtonTextOnly);
	m_button->setMinimumWidth(132);

	m_menu = new QMenu(m_button);
	m_none_action = m_menu->addAction(tr("无"));
	m_menu->addSeparator();
	m_all_types_action = m_menu->addAction(tr("全部类型"));
	m_static_mesh_action = m_menu->addAction(tr("Static Mesh"));
	m_skeletal_mesh_action = m_menu->addAction(tr("Skeletal Mesh"));
	m_other_action = m_menu->addAction(tr("其它"));

	m_all_types_action->setCheckable(true);
	m_static_mesh_action->setCheckable(true);
	m_skeletal_mesh_action->setCheckable(true);
	m_other_action->setCheckable(true);

	m_button->setMenu(m_menu);

	connect(m_none_action, &QAction::triggered, this, &AssetBrowserTypeFilterWidget::OnNoneTriggered);
	connect(m_all_types_action, &QAction::triggered, this, &AssetBrowserTypeFilterWidget::OnAllTypesTriggered);
	connect(
		m_static_mesh_action,
		&QAction::triggered,
		this,
		&AssetBrowserTypeFilterWidget::OnSpecificTypeTriggered);
	connect(
		m_skeletal_mesh_action,
		&QAction::triggered,
		this,
		&AssetBrowserTypeFilterWidget::OnSpecificTypeTriggered);
	connect(m_other_action, &QAction::triggered, this, &AssetBrowserTypeFilterWidget::OnSpecificTypeTriggered);

	RootLayout->addWidget(m_button);
	ApplyNoneMode();
}

bool AssetBrowserTypeFilterWidget::IsNoneMode() const
{
	return m_b_none_mode;
}

bool AssetBrowserTypeFilterWidget::IsAllTypesMode() const
{
	return !m_b_none_mode && m_all_types_action->isChecked();
}

QSet<QString> AssetBrowserTypeFilterWidget::GetSelectedTypeFilters() const
{
	QSet<QString> SelectedFilters;
	if (m_b_none_mode || m_all_types_action->isChecked())
	{
		return SelectedFilters;
	}

	if (m_static_mesh_action->isChecked())
	{
		SelectedFilters.insert(QStringLiteral("StaticMesh"));
	}
	if (m_skeletal_mesh_action->isChecked())
	{
		SelectedFilters.insert(QStringLiteral("SkeletalMesh"));
	}
	if (m_other_action->isChecked())
	{
		SelectedFilters.insert(QString::fromLatin1(kTypeFilterOther));
	}

	return SelectedFilters;
}

bool AssetBrowserTypeFilterWidget::MatchesAssetType(const std::string& InAssetType) const
{
	if (m_b_none_mode || m_all_types_action->isChecked())
	{
		return true;
	}

	const QSet<QString> SelectedFilters = GetSelectedTypeFilters();
	if (SelectedFilters.isEmpty())
	{
		return true;
	}

	const QString AssetType = QString::fromStdString(InAssetType);
	for (const QString& Filter : SelectedFilters)
	{
		if (Filter == QString::fromLatin1(kTypeFilterOther))
		{
			if (AssetType != QLatin1String("StaticMesh") && AssetType != QLatin1String("SkeletalMesh"))
			{
				return true;
			}
		}
		else if (AssetType == Filter)
		{
			return true;
		}
	}

	return false;
}

void AssetBrowserTypeFilterWidget::SetNoneMode()
{
	QSignalBlocker Blocker(this);
	ApplyNoneMode();
}

void AssetBrowserTypeFilterWidget::SetAllTypesMode()
{
	QSignalBlocker Blocker(this);
	ApplyAllTypesMode();
}

void AssetBrowserTypeFilterWidget::ApplyNoneMode()
{
	m_b_none_mode = true;
	m_all_types_action->setChecked(false);
	m_static_mesh_action->setChecked(false);
	m_skeletal_mesh_action->setChecked(false);
	m_other_action->setChecked(false);
	UpdateButtonText();
}

void AssetBrowserTypeFilterWidget::ApplyAllTypesMode()
{
	m_b_none_mode = false;
	m_all_types_action->setChecked(true);
	m_static_mesh_action->setChecked(false);
	m_skeletal_mesh_action->setChecked(false);
	m_other_action->setChecked(false);
	UpdateButtonText();
}

void AssetBrowserTypeFilterWidget::UpdateButtonText()
{
	if (m_b_none_mode)
	{
		m_button->setText(tr("无"));
		return;
	}

	if (m_all_types_action->isChecked())
	{
		m_button->setText(tr("全部类型"));
		return;
	}

	QStringList Labels;
	if (m_static_mesh_action->isChecked())
	{
		Labels.push_back(tr("Static Mesh"));
	}
	if (m_skeletal_mesh_action->isChecked())
	{
		Labels.push_back(tr("Skeletal Mesh"));
	}
	if (m_other_action->isChecked())
	{
		Labels.push_back(tr("其它"));
	}

	if (Labels.isEmpty())
	{
		m_button->setText(tr("全部类型"));
		return;
	}

	if (Labels.size() == 1)
	{
		m_button->setText(Labels.front());
		return;
	}

	if (Labels.size() == 2)
	{
		m_button->setText(Labels.join(QStringLiteral(", ")));
		return;
	}

	m_button->setText(tr("%1 +%2").arg(Labels.front()).arg(Labels.size() - 1));
}

void AssetBrowserTypeFilterWidget::OnNoneTriggered()
{
	if (m_b_none_mode)
	{
		return;
	}

	ApplyNoneMode();
	emit FilterChanged();
}

void AssetBrowserTypeFilterWidget::OnAllTypesTriggered(bool bChecked)
{
	if (!bChecked)
	{
		m_all_types_action->setChecked(true);
		return;
	}

	m_b_none_mode = false;
	m_static_mesh_action->setChecked(false);
	m_skeletal_mesh_action->setChecked(false);
	m_other_action->setChecked(false);
	UpdateButtonText();
	emit FilterChanged();
}

void AssetBrowserTypeFilterWidget::OnSpecificTypeTriggered()
{
	m_b_none_mode = false;
	m_all_types_action->setChecked(false);

	if (!m_static_mesh_action->isChecked()
		&& !m_skeletal_mesh_action->isChecked()
		&& !m_other_action->isChecked())
	{
		ApplyAllTypesMode();
		emit FilterChanged();
		return;
	}

	UpdateButtonText();
	emit FilterChanged();
}
