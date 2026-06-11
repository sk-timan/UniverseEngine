#pragma once

#include <QSet>
#include <QString>
#include <QWidget>

class QAction;
class QMenu;
class QToolButton;

class AssetBrowserTypeFilterWidget final : public QWidget
{
	Q_OBJECT

public:
	explicit AssetBrowserTypeFilterWidget(QWidget* InParent = nullptr);

	bool IsNoneMode() const;
	bool IsAllTypesMode() const;
	QSet<QString> GetSelectedTypeFilters() const;
	bool MatchesAssetType(const std::string& InAssetType) const;

	void SetNoneMode();
	void SetAllTypesMode();

signals:
	void FilterChanged();

private:
	void ApplyNoneMode();
	void ApplyAllTypesMode();
	void UpdateButtonText();
	void OnNoneTriggered();
	void OnAllTypesTriggered(bool bChecked);
	void OnSpecificTypeTriggered();

	QToolButton* m_button = nullptr;
	QMenu* m_menu = nullptr;
	QAction* m_none_action = nullptr;
	QAction* m_all_types_action = nullptr;
	QAction* m_static_mesh_action = nullptr;
	QAction* m_skeletal_mesh_action = nullptr;
	QAction* m_texture2d_action = nullptr;
	QAction* m_other_action = nullptr;
	bool m_b_none_mode = true;
};
