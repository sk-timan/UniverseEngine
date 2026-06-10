#pragma once

#include <windows.h>

#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QWidget>

#include <functional>

#include "data/GameplayConfig.h"

class QCloseEvent;
class QEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QFocusEvent;
class QMouseEvent;
class QWheelEvent;
class QResizeEvent;
class QShowEvent;

class AssetBrowserPanelWidget;
class CameraSpeedControlWidget;
class DetailPanelWidget;
class GameApp;
class OutputLogPanelWidget;
class WorldContentPanelWidget;

class RenderViewportWidget final : public QWidget
{
	Q_OBJECT

public:
	explicit RenderViewportWidget(GameApp* InGameApp, QWidget* InParent = nullptr);
	void FlushPendingResizeIfStable();

protected:
	void dragEnterEvent(QDragEnterEvent* InEvent) override;
	void dragMoveEvent(QDragMoveEvent* InEvent) override;
	void dropEvent(QDropEvent* InEvent) override;
	void focusInEvent(QFocusEvent* InEvent) override;
	void mousePressEvent(QMouseEvent* InEvent) override;
	void mouseMoveEvent(QMouseEvent* InEvent) override;
	void mouseReleaseEvent(QMouseEvent* InEvent) override;
	void wheelEvent(QWheelEvent* InEvent) override;
	void resizeEvent(QResizeEvent* InEvent) override;
	void focusOutEvent(QFocusEvent* InEvent) override;

private:
	GameApp* m_game_app_ = nullptr;
	qint64 m_last_resize_event_ms_ = 0;
	int m_pending_width_ = 0;
	int m_pending_height_ = 0;
	bool m_has_pending_resize_ = false;
};

class ViewportHostWidget final : public QWidget
{
	Q_OBJECT

public:
	explicit ViewportHostWidget(GameApp* InGameApp, QWidget* InParent = nullptr);

	RenderViewportWidget* GetViewportWidget() const;
	void FlushPendingResizeIfStable();
	void SyncCameraSpeedControlFromGameApp();
	void SyncRotateDragLabelFromGameApp();

protected:
	void showEvent(QShowEvent* InEvent) override;
	void resizeEvent(QResizeEvent* InEvent) override;

private:
	void SetupCameraSpeedControl();
	void RaiseOverlayWidgets();

	GameApp* m_game_app_ = nullptr;
	QPointer<RenderViewportWidget> m_viewport_;
	QPointer<CameraSpeedControlWidget> m_camera_speed_control_;
	QPointer<QLabel> m_rotate_drag_label_;
};

class MainWindow final : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(GameApp* InGameApp);
	~MainWindow() override;

	HWND GetRenderWindowHandle() const;
	int GetRenderWidth() const;
	int GetRenderHeight() const;
	void StartFrameLoop();

protected:
	void closeEvent(QCloseEvent* InEvent) override;
	bool eventFilter(QObject* InWatched, QEvent* InEvent) override;

private slots:
	void OnFrameTick();

private:
	void BuildEditorPanel();
	void BuildDisplayPanel();
	void BuildWorldContentPanel();
	void BuildDetailPanel();
	void BuildAssetBrowserPanel();
	void BuildOutputLogPanel();
	void ConfigureDockLayout();
	void ApplyInitialDockProportions();
	void RefreshScenePanels();
	void RefreshAssetBrowser();
	void UpdateRuntimeDisplay();
	void BuildMenuBar();
	void LoadGameplayConfigToEditor();
	void PopulateGameplayConfigControls(const GameplayConfig& InConfig);
	bool BuildGameplayConfigFromControls(GameplayConfig* OutConfig, QString* OutErrorMessage) const;
	void SetEditorStatus(const QString& InMessage, bool bIsError);
	void UpdateEditorActionState();
	void OnAnyConfigFieldChanged();
	void OnNewMapClicked();
	void OnOpenMapClicked();
	void OnSaveMapClicked();
	void OnSaveAsMapClicked();
	void OnImportAssetClicked();
	void OnLoadModelClicked();
	void OnEditorPreferencesClicked();
	void OnEditorPerformanceClicked();
	void OnExitClicked();
	void OnDeleteSelectedActorTriggered();
	void SyncViewportCameraSpeedControl();

	GameApp* m_game_app_ = nullptr;
	QPointer<ViewportHostWidget> m_viewport_host_;
	QPointer<QTimer> m_frame_timer_;
	QPointer<QMenuBar> m_menu_bar_;
	QPointer<QAction> m_new_map_action_;
	QPointer<QAction> m_open_map_action_;
	QPointer<QAction> m_save_map_action_;
	QPointer<QAction> m_save_as_map_action_;
	QPointer<QAction> m_import_asset_action_;
	QPointer<QAction> m_load_model_action_;
	QPointer<QAction> m_editor_preferences_action_;
	QPointer<QAction> m_editor_performance_action_;
	QPointer<QLabel> m_runtime_hint_label_;
	QPointer<WorldContentPanelWidget> m_world_content_panel_;
	QPointer<DetailPanelWidget> m_detail_panel_;
	QPointer<AssetBrowserPanelWidget> m_asset_browser_panel_;
	QPointer<OutputLogPanelWidget> m_output_log_panel_;
	QPointer<QLineEdit> m_map_id_edit_;
	QPointer<QDoubleSpinBox> m_spawn_x_spin_;
	QPointer<QDoubleSpinBox> m_spawn_y_spin_;
	QPointer<QDoubleSpinBox> m_spawn_z_spin_;
	QPointer<QSpinBox> m_starting_coins_spin_;
	QPointer<QLabel> m_editor_status_label_;
	GameplayConfig m_loaded_gameplay_config_{};
	bool m_editor_dirty_ = false;
	bool m_editor_controls_syncing_ = false;
};
