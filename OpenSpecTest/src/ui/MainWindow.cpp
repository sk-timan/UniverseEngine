#include "ui/MainWindow.h"

#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QGridLayout>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStyle>
#include <QTimer>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

#include <filesystem>

#include "app/GameApp.h"
#include "ui/CameraSpeedControlWidget.h"
#include "ui/EditorPerformanceDialog.h"
#include "ui/EditorPreferencesDialog.h"
#include "ui/DetailPanelWidget.h"
#include "asset/ProjectPaths.h"
#include "ui/ImportModelTransformDialog.h"
#include "ui/WorldContentPanelWidget.h"
#include "data/GameplayConfig.h"

namespace
{
QPoint MapViewportMouseToPhysical(const QWidget* InWidget, const QPointF& InPosition)
{
	const qreal DevicePixelRatio = (InWidget != nullptr) ? InWidget->devicePixelRatioF() : 1.0;
	return QPoint(
		static_cast<int>(InPosition.x() * DevicePixelRatio),
		static_cast<int>(InPosition.y() * DevicePixelRatio));
}
} // namespace

RenderViewportWidget::RenderViewportWidget(GameApp* InGameApp, QWidget* InParent)
	: QWidget(InParent)
	, m_game_app_(InGameApp)
{
	setObjectName("RenderViewportWidget");
	setFocusPolicy(Qt::StrongFocus);
	setMouseTracking(true);
	setAttribute(Qt::WA_NativeWindow, true);
	setAttribute(Qt::WA_DontCreateNativeAncestors, true);
}

ViewportHostWidget::ViewportHostWidget(GameApp* InGameApp, QWidget* InParent)
	: QWidget(InParent)
	, m_game_app_(InGameApp)
{
	setObjectName("ViewportHostWidget");

	auto* Layout = new QGridLayout(this);
	Layout->setContentsMargins(0, 0, 0, 0);
	Layout->setSpacing(0);

	m_viewport_ = new RenderViewportWidget(m_game_app_, this);
	Layout->addWidget(m_viewport_, 0, 0);

	m_rotate_drag_label_ = new QLabel(this);
	m_rotate_drag_label_->setObjectName("RotateDragLabel");
	m_rotate_drag_label_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	m_rotate_drag_label_->setStyleSheet(
		"QLabel#RotateDragLabel { color: #f5f5f5; background-color: rgba(0, 0, 0, 160); padding: 2px 6px; border-radius: 3px; }");
	m_rotate_drag_label_->setFont(QFont("Segoe UI", 9, QFont::DemiBold));
	m_rotate_drag_label_->hide();

	SetupCameraSpeedControl();
	if (m_camera_speed_control_)
	{
		Layout->addWidget(m_camera_speed_control_, 0, 0, Qt::AlignTop | Qt::AlignRight);
	}
}

RenderViewportWidget* ViewportHostWidget::GetViewportWidget() const
{
	return m_viewport_;
}

void ViewportHostWidget::FlushPendingResizeIfStable()
{
	if (m_viewport_)
	{
		m_viewport_->FlushPendingResizeIfStable();
	}
}

void ViewportHostWidget::showEvent(QShowEvent* InEvent)
{
	QWidget::showEvent(InEvent);
	if (m_camera_speed_control_)
	{
		m_camera_speed_control_->raise();
		QTimer::singleShot(0, this, [this]()
		{
			if (m_camera_speed_control_)
			{
				m_camera_speed_control_->raise();
			}
		});
	}
}

void ViewportHostWidget::resizeEvent(QResizeEvent* InEvent)
{
	QWidget::resizeEvent(InEvent);
	if (m_camera_speed_control_)
	{
		m_camera_speed_control_->raise();
	}
}

void ViewportHostWidget::SyncRotateDragLabelFromGameApp()
{
	if (m_game_app_ == nullptr || m_rotate_drag_label_ == nullptr)
	{
		return;
	}

	const FEditorRotateDragLabel& Label = m_game_app_->GetRotateDragLabel();
	if (!Label.bVisible)
	{
		m_rotate_drag_label_->hide();
		return;
	}

	int WidgetX = 0;
	int WidgetY = 0;
	m_game_app_->MapPickScreenToViewportWidget(Label.ScreenX, Label.ScreenY, &WidgetX, &WidgetY);
	m_rotate_drag_label_->setText(QString::number(static_cast<double>(Label.AngleDegrees), 'f', 2));
	m_rotate_drag_label_->adjustSize();
	m_rotate_drag_label_->move(WidgetX + 10, WidgetY - m_rotate_drag_label_->height() / 2);
	m_rotate_drag_label_->show();
	m_rotate_drag_label_->raise();
}

void ViewportHostWidget::SyncCameraSpeedControlFromGameApp()
{
	if (m_game_app_ == nullptr || m_camera_speed_control_ == nullptr)
	{
		return;
	}

	m_camera_speed_control_->SetCameraSpeed(static_cast<int>(m_game_app_->GetCameraMoveSpeed()));
	m_camera_speed_control_->SetCameraSpeedScalar(m_game_app_->GetCameraSpeedScalar());
}

void ViewportHostWidget::SetupCameraSpeedControl()
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	m_camera_speed_control_ = new CameraSpeedControlWidget(this);
	m_camera_speed_control_->setObjectName("CameraSpeedControlRoot");
	m_camera_speed_control_->setAttribute(Qt::WA_NativeWindow, true);
	m_camera_speed_control_->setAttribute(Qt::WA_DontCreateNativeAncestors, true);
	m_camera_speed_control_->setAttribute(Qt::WA_NoSystemBackground, false);
	m_camera_speed_control_->raise();

	SyncCameraSpeedControlFromGameApp();

	connect(
		m_camera_speed_control_,
		&CameraSpeedControlWidget::CameraSpeedChanged,
		this,
		[this](int InSpeed)
		{
			if (m_game_app_ != nullptr)
			{
				m_game_app_->SetCameraMoveSpeed(static_cast<float>(InSpeed));
			}
		});
	connect(
		m_camera_speed_control_,
		&CameraSpeedControlWidget::CameraSpeedScalarChanged,
		this,
		[this](float InScalar)
		{
			if (m_game_app_ != nullptr)
			{
				m_game_app_->SetCameraSpeedScalar(InScalar);
			}
		});
}

void RenderViewportWidget::focusInEvent(QFocusEvent* InEvent)
{
	if (m_game_app_ != nullptr)
	{
		m_game_app_->OnFocusGained();
	}
	QWidget::focusInEvent(InEvent);
}

void RenderViewportWidget::mousePressEvent(QMouseEvent* InEvent)
{
	if (!hasFocus())
	{
		setFocus(Qt::MouseFocusReason);
	}
	if (m_game_app_ != nullptr)
	{
		if (InEvent->button() == Qt::RightButton)
		{
			m_game_app_->SetMouseLookActive(true, false);
		}
		else if (InEvent->button() == Qt::LeftButton)
		{
			const QPoint PhysicalPos = MapViewportMouseToPhysical(this, InEvent->position());
			m_game_app_->OnViewportLeftMousePress(PhysicalPos.x(), PhysicalPos.y());
		}
	}
	QWidget::mousePressEvent(InEvent);
}

void RenderViewportWidget::mouseMoveEvent(QMouseEvent* InEvent)
{
	if (m_game_app_ != nullptr)
	{
		const QPoint PhysicalPos = MapViewportMouseToPhysical(this, InEvent->position());
		m_game_app_->OnViewportLeftMouseMove(PhysicalPos.x(), PhysicalPos.y());
	}
	QWidget::mouseMoveEvent(InEvent);
}

void RenderViewportWidget::mouseReleaseEvent(QMouseEvent* InEvent)
{
	if (m_game_app_ != nullptr)
	{
		if (InEvent->button() == Qt::RightButton)
		{
			m_game_app_->SetMouseLookActive(false, true);
		}
		else if (InEvent->button() == Qt::LeftButton)
		{
			const QPoint PhysicalPos = MapViewportMouseToPhysical(this, InEvent->position());
			m_game_app_->OnViewportLeftMouseRelease(PhysicalPos.x(), PhysicalPos.y());
		}
	}
	QWidget::mouseReleaseEvent(InEvent);
}

void RenderViewportWidget::resizeEvent(QResizeEvent* InEvent)
{
	const qreal DevicePixelRatio = devicePixelRatioF();
	m_pending_width_ = static_cast<int>(InEvent->size().width() * DevicePixelRatio);
	m_pending_height_ = static_cast<int>(InEvent->size().height() * DevicePixelRatio);
	m_last_resize_event_ms_ = QDateTime::currentMSecsSinceEpoch();
	m_has_pending_resize_ = true;
	QWidget::resizeEvent(InEvent);
}

void RenderViewportWidget::focusOutEvent(QFocusEvent* InEvent)
{
	if (m_game_app_ != nullptr)
	{
		m_game_app_->OnFocusLost();
	}
	QWidget::focusOutEvent(InEvent);
}

void RenderViewportWidget::FlushPendingResizeIfStable()
{
	if (!m_has_pending_resize_ || m_game_app_ == nullptr)
	{
		return;
	}
	if (QApplication::mouseButtons() != Qt::NoButton)
	{
		return;
	}

	constexpr qint64 kResizeSettleMs = 140;
	const qint64 NowMs = QDateTime::currentMSecsSinceEpoch();
	if ((NowMs - m_last_resize_event_ms_) < kResizeSettleMs)
	{
		return;
	}

	if (m_pending_width_ <= 0 || m_pending_height_ <= 0)
	{
		m_game_app_->OnResize(0, 0);
	}
	else
	{
		m_game_app_->OnResize(
			static_cast<UINT>(m_pending_width_),
			static_cast<UINT>(m_pending_height_));
	}
	m_has_pending_resize_ = false;
}

MainWindow::MainWindow(GameApp* InGameApp)
	: m_game_app_(InGameApp)
{
	resize(1480, 840);
	setWindowTitle("OpenSpecTest - DX12 MVP (Qt UI)");

	m_viewport_host_ = new ViewportHostWidget(m_game_app_, this);
	setCentralWidget(m_viewport_host_);

	BuildMenuBar();
	BuildEditorPanel();
	LoadGameplayConfigToEditor();
	BuildWorldContentPanel();
	BuildDetailPanel();
	BuildDisplayPanel();
	SyncViewportCameraSpeedControl();

	QAction* DeleteSelectedActorAction = new QAction(this);
	DeleteSelectedActorAction->setShortcut(QKeySequence::Delete);
	DeleteSelectedActorAction->setShortcutContext(Qt::ApplicationShortcut);
	connect(
		DeleteSelectedActorAction,
		&QAction::triggered,
		this,
		&MainWindow::OnDeleteSelectedActorTriggered);
	addAction(DeleteSelectedActorAction);
}

HWND MainWindow::GetRenderWindowHandle() const
{
	if (!m_viewport_host_)
	{
		return nullptr;
	}
	RenderViewportWidget* Viewport = m_viewport_host_->GetViewportWidget();
	if (Viewport == nullptr)
	{
		return nullptr;
	}
	return reinterpret_cast<HWND>(Viewport->winId());
}

int MainWindow::GetRenderWidth() const
{
	RenderViewportWidget* Viewport =
		(m_viewport_host_) ? m_viewport_host_->GetViewportWidget() : nullptr;
	return (Viewport != nullptr) ? Viewport->width() : 0;
}

int MainWindow::GetRenderHeight() const
{
	RenderViewportWidget* Viewport =
		(m_viewport_host_) ? m_viewport_host_->GetViewportWidget() : nullptr;
	return (Viewport != nullptr) ? Viewport->height() : 0;
}

void MainWindow::StartFrameLoop()
{
	if (m_frame_timer_)
	{
		return;
	}

	m_frame_timer_ = new QTimer(this);
	m_frame_timer_->setInterval(16);
	connect(m_frame_timer_, &QTimer::timeout, this, &MainWindow::OnFrameTick);
	m_frame_timer_->start();
}

void MainWindow::closeEvent(QCloseEvent* InEvent)
{
	if (m_game_app_ != nullptr)
	{
		m_game_app_->OnFocusLost();
	}
	QMainWindow::closeEvent(InEvent);
}

void MainWindow::OnFrameTick()
{
	if (m_viewport_host_)
	{
		m_viewport_host_->FlushPendingResizeIfStable();
	}
	if (m_game_app_ != nullptr)
	{
		m_game_app_->Tick();
	}
	if (m_viewport_host_ != nullptr)
	{
		m_viewport_host_->SyncRotateDragLabelFromGameApp();
	}
	RefreshScenePanels();
	UpdateRuntimeDisplay();
}

void MainWindow::BuildEditorPanel()
{
	auto* EditorDock = new QDockWidget(tr("编辑面板（Qt）"), this);
	EditorDock->setObjectName("EditorDock");
	EditorDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

	auto* Container = new QWidget(EditorDock);
	auto* Layout = new QVBoxLayout(Container);
	Layout->setContentsMargins(10, 10, 10, 10);
	Layout->setSpacing(10);

	auto* TackleGroup = new QGroupBox(tr("Gameplay 配置"), Container);
	auto* TackleForm = new QFormLayout(TackleGroup);

	m_map_id_edit_ = new QLineEdit(TackleGroup);
	m_map_id_edit_->setPlaceholderText(tr("例如: mvp_pond_01"));

	m_spawn_x_spin_ = new QDoubleSpinBox(TackleGroup);
	m_spawn_y_spin_ = new QDoubleSpinBox(TackleGroup);
	m_spawn_z_spin_ = new QDoubleSpinBox(TackleGroup);
	for (QDoubleSpinBox* Spin : {m_spawn_x_spin_.data(), m_spawn_y_spin_.data(), m_spawn_z_spin_.data()})
	{
		Spin->setDecimals(3);
		Spin->setRange(-100000.0, 100000.0);
		Spin->setSingleStep(0.1);
	}

	m_starting_coins_spin_ = new QSpinBox(TackleGroup);
	m_starting_coins_spin_->setRange(0, 1000000000);

	TackleForm->addRow(tr("地图 ID"), m_map_id_edit_);
	TackleForm->addRow(tr("出生点 X"), m_spawn_x_spin_);
	TackleForm->addRow(tr("出生点 Y"), m_spawn_y_spin_);
	TackleForm->addRow(tr("出生点 Z"), m_spawn_z_spin_);
	TackleForm->addRow(tr("初始金币"), m_starting_coins_spin_);

	m_editor_status_label_ = new QLabel(tr("已加载 gameplay 配置。"), Container);
	m_editor_status_label_->setObjectName("EditorStatusLabel");
	m_editor_status_label_->setWordWrap(true);
	m_editor_status_label_->setFrameShape(QFrame::NoFrame);

	connect(m_map_id_edit_, &QLineEdit::textEdited, this, [this](const QString&)
	{
		OnAnyConfigFieldChanged();
	});
	connect(m_spawn_x_spin_, &QDoubleSpinBox::valueChanged, this, [this](double)
	{
		OnAnyConfigFieldChanged();
	});
	connect(m_spawn_y_spin_, &QDoubleSpinBox::valueChanged, this, [this](double)
	{
		OnAnyConfigFieldChanged();
	});
	connect(m_spawn_z_spin_, &QDoubleSpinBox::valueChanged, this, [this](double)
	{
		OnAnyConfigFieldChanged();
	});
	connect(m_starting_coins_spin_, &QSpinBox::valueChanged, this, [this](int)
	{
		OnAnyConfigFieldChanged();
	});

	Layout->addWidget(TackleGroup);
	Layout->addWidget(m_editor_status_label_);
	Layout->addStretch(1);

	EditorDock->setWidget(Container);
	addDockWidget(Qt::LeftDockWidgetArea, EditorDock);
}

void MainWindow::LoadGameplayConfigToEditor()
{
	if (m_game_app_ == nullptr)
	{
		return;
	}
	m_loaded_gameplay_config_ = m_game_app_->GetGameplayConfig();
	PopulateGameplayConfigControls(m_loaded_gameplay_config_);
	m_editor_dirty_ = false;
	SetEditorStatus(tr("已从运行时加载 gameplay 配置。"), false);
	UpdateEditorActionState();
}

void MainWindow::PopulateGameplayConfigControls(const GameplayConfig& InConfig)
{
	m_editor_controls_syncing_ = true;
	if (m_map_id_edit_)
	{
		m_map_id_edit_->setText(QString::fromStdString(InConfig.map_id));
	}
	if (m_spawn_x_spin_)
	{
		m_spawn_x_spin_->setValue(InConfig.spawn_x);
	}
	if (m_spawn_y_spin_)
	{
		m_spawn_y_spin_->setValue(InConfig.spawn_y);
	}
	if (m_spawn_z_spin_)
	{
		m_spawn_z_spin_->setValue(InConfig.spawn_z);
	}
	if (m_starting_coins_spin_)
	{
		m_starting_coins_spin_->setValue(InConfig.starting_coins);
	}
	m_editor_controls_syncing_ = false;
}

bool MainWindow::BuildGameplayConfigFromControls(GameplayConfig* OutConfig, QString* OutErrorMessage) const
{
	if (OutConfig == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = tr("内部错误：配置输出对象为空。");
		}
		return false;
	}
	if (!m_map_id_edit_ || !m_spawn_x_spin_ || !m_spawn_y_spin_ ||
		!m_spawn_z_spin_ || !m_starting_coins_spin_)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = tr("配置控件尚未初始化。");
		}
		return false;
	}

	GameplayConfig Config = m_loaded_gameplay_config_;
	Config.map_id = m_map_id_edit_->text().trimmed().toStdString();
	Config.spawn_x = static_cast<float>(m_spawn_x_spin_->value());
	Config.spawn_y = static_cast<float>(m_spawn_y_spin_->value());
	Config.spawn_z = static_cast<float>(m_spawn_z_spin_->value());
	Config.starting_coins = m_starting_coins_spin_->value();

	if (Config.map_id.empty())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = tr("地图 ID 不能为空。");
		}
		return false;
	}
	if (Config.starting_coins < 0)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = tr("初始金币不能为负数。");
		}
		return false;
	}

	*OutConfig = Config;
	return true;
}

void MainWindow::SetEditorStatus(const QString& InMessage, bool bIsError)
{
	if (!m_editor_status_label_)
	{
		return;
	}
	m_editor_status_label_->setText(InMessage);
	m_editor_status_label_->setProperty("status", bIsError ? "error" : "success");
	m_editor_status_label_->style()->unpolish(m_editor_status_label_);
	m_editor_status_label_->style()->polish(m_editor_status_label_);
}

void MainWindow::UpdateEditorActionState()
{
	// Editor action state update - buttons removed, using menu instead
}

void MainWindow::OnAnyConfigFieldChanged()
{
	if (m_editor_controls_syncing_)
	{
		return;
	}
	m_editor_dirty_ = true;
	SetEditorStatus(tr("存在未保存更改。"), false);
}

void MainWindow::BuildDisplayPanel()
{
	auto* DisplayDock = new QDockWidget(tr("显示面板（Qt）"), this);
	DisplayDock->setObjectName("DisplayDock");
	DisplayDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

	auto* Container = new QWidget(DisplayDock);
	auto* Layout = new QVBoxLayout(Container);
	Layout->setContentsMargins(10, 10, 10, 10);
	Layout->setSpacing(10);

	auto* RuntimeGroup = new QGroupBox(tr("运行状态"), Container);
	auto* RuntimeLayout = new QVBoxLayout(RuntimeGroup);
	m_runtime_hint_label_ = new QLabel(
		tr("渲染循环已切换至 Qt 定时驱动，右键按住进入鼠标视角。"), RuntimeGroup);
	m_runtime_hint_label_->setWordWrap(true);
	RuntimeLayout->addWidget(m_runtime_hint_label_);

	auto* SceneLabel = new QLabel(tr("场景: MVP池塘区域（占位）"), Container);
	SceneLabel->setFrameShape(QFrame::StyledPanel);

	Layout->addWidget(RuntimeGroup);
	Layout->addWidget(SceneLabel);
	Layout->addStretch(1);

	DisplayDock->setWidget(Container);
	addDockWidget(Qt::RightDockWidgetArea, DisplayDock);

	if (auto* WorldContentDock = findChild<QDockWidget*>("WorldContentDock"))
	{
		tabifyDockWidget(WorldContentDock, DisplayDock);
		WorldContentDock->raise();
	}
}

void MainWindow::BuildWorldContentPanel()
{
	auto* WorldContentDock = new QDockWidget(tr("WorldContent"), this);
	WorldContentDock->setObjectName("WorldContentDock");
	WorldContentDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

	m_world_content_panel_ = new WorldContentPanelWidget(m_game_app_, WorldContentDock);
	WorldContentDock->setWidget(m_world_content_panel_);
	addDockWidget(Qt::RightDockWidgetArea, WorldContentDock);
}

void MainWindow::BuildDetailPanel()
{
	auto* DetailDock = new QDockWidget(tr("Detail"), this);
	DetailDock->setObjectName("DetailDock");
	DetailDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

	m_detail_panel_ = new DetailPanelWidget(m_game_app_, DetailDock);
	DetailDock->setWidget(m_detail_panel_);
	addDockWidget(Qt::RightDockWidgetArea, DetailDock);

	if (auto* WorldContentDock = findChild<QDockWidget*>("WorldContentDock"))
	{
		splitDockWidget(WorldContentDock, DetailDock, Qt::Vertical);
	}
}

void MainWindow::RefreshScenePanels()
{
	if (m_world_content_panel_)
	{
		m_world_content_panel_->RefreshFromScene();
	}
	if (m_detail_panel_)
	{
		m_detail_panel_->RefreshFromSelection();
	}
}

void MainWindow::BuildMenuBar()
{
	m_menu_bar_ = new QMenuBar(this);
	setMenuBar(m_menu_bar_);

	auto* FileMenu = new QMenu(tr("文件(&F)"), m_menu_bar_);
	m_menu_bar_->addMenu(FileMenu);

	m_new_map_action_ = new QAction(tr("新建地图(&N)"), FileMenu);
	m_new_map_action_->setShortcut(QKeySequence::New);
	connect(m_new_map_action_, &QAction::triggered, this, &MainWindow::OnNewMapClicked);
	FileMenu->addAction(m_new_map_action_);

	m_open_map_action_ = new QAction(tr("打开地图(&O)"), FileMenu);
	m_open_map_action_->setShortcut(QKeySequence::Open);
	connect(m_open_map_action_, &QAction::triggered, this, &MainWindow::OnOpenMapClicked);
	FileMenu->addAction(m_open_map_action_);

	FileMenu->addSeparator();

	m_import_asset_action_ = new QAction(tr("导入(&I)..."), FileMenu);
	connect(m_import_asset_action_, &QAction::triggered, this, &MainWindow::OnImportAssetClicked);
	FileMenu->addAction(m_import_asset_action_);

	m_load_model_action_ = new QAction(tr("加载模型(&L)..."), FileMenu);
	connect(m_load_model_action_, &QAction::triggered, this, &MainWindow::OnLoadModelClicked);
	FileMenu->addAction(m_load_model_action_);

	FileMenu->addSeparator();

	m_save_map_action_ = new QAction(tr("保存地图(&S)"), FileMenu);
	m_save_map_action_->setShortcut(QKeySequence::Save);
	connect(m_save_map_action_, &QAction::triggered, this, &MainWindow::OnSaveMapClicked);
	FileMenu->addAction(m_save_map_action_);

	m_save_as_map_action_ = new QAction(tr("另存为(&A)..."), FileMenu);
	m_save_as_map_action_->setShortcut(QKeySequence::SaveAs);
	connect(m_save_as_map_action_, &QAction::triggered, this, &MainWindow::OnSaveAsMapClicked);
	FileMenu->addAction(m_save_as_map_action_);

	FileMenu->addSeparator();

	auto* ExitAction = new QAction(tr("退出(&X)"), FileMenu);
	ExitAction->setShortcut(QKeySequence::Quit);
	connect(ExitAction, &QAction::triggered, this, &MainWindow::OnExitClicked);
	FileMenu->addAction(ExitAction);

	auto* EditMenu = new QMenu(tr("编辑(&E)"), m_menu_bar_);
	m_menu_bar_->addMenu(EditMenu);

	m_editor_preferences_action_ = new QAction(tr("编辑器偏好设置..."), EditMenu);
	connect(m_editor_preferences_action_, &QAction::triggered, this, &MainWindow::OnEditorPreferencesClicked);
	EditMenu->addAction(m_editor_preferences_action_);

	m_editor_performance_action_ = new QAction(tr("性能(&P)..."), EditMenu);
	connect(m_editor_performance_action_, &QAction::triggered, this, &MainWindow::OnEditorPerformanceClicked);
	EditMenu->addAction(m_editor_performance_action_);
}

void MainWindow::SyncViewportCameraSpeedControl()
{
	if (m_viewport_host_ == nullptr || m_game_app_ == nullptr)
	{
		return;
	}

	m_viewport_host_->SyncCameraSpeedControlFromGameApp();
}

void MainWindow::OnEditorPreferencesClicked()
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	EditorPreferencesDialog Dialog(m_game_app_->GetEditorPreferences(), this);
	if (Dialog.exec() != QDialog::Accepted)
	{
		return;
	}

	const FEditorPreferences NewPreferences = Dialog.GetPreferences();
	std::string ApplyError;
	if (!m_game_app_->ApplyEditorPreferences(NewPreferences, &ApplyError))
	{
		QMessageBox::warning(
			this,
			tr("编辑器偏好设置"),
			tr("无法应用设置：%1").arg(QString::fromStdString(ApplyError)));
		return;
	}

	std::string SaveError;
	if (!m_game_app_->SaveEditorPreferences(&SaveError))
	{
		QMessageBox::warning(
			this,
			tr("编辑器偏好设置"),
			tr("设置已应用，但保存到配置文件失败：%1").arg(QString::fromStdString(SaveError)));
	}

	SyncViewportCameraSpeedControl();
}

void MainWindow::OnEditorPerformanceClicked()
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	EditorPerformanceDialog Dialog(m_game_app_->GetEditorPerformanceSettings(), this);
	if (Dialog.exec() != QDialog::Accepted)
	{
		return;
	}

	const FEditorPerformanceSettings NewSettings = Dialog.GetSettings();
	std::string ApplyError;
	if (!m_game_app_->ApplyEditorPerformanceSettings(NewSettings, &ApplyError))
	{
		QMessageBox::warning(
			this,
			tr("性能"),
			tr("无法应用设置：%1").arg(QString::fromStdString(ApplyError)));
		return;
	}

	std::string SaveError;
	if (!m_game_app_->SaveEditorPerformanceSettings(&SaveError))
	{
		QMessageBox::warning(
			this,
			tr("性能"),
			tr("设置已应用，但保存到配置文件失败：%1").arg(QString::fromStdString(SaveError)));
	}
}

void MainWindow::OnNewMapClicked()
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	QString DefaultName = QString("new_map_%1").arg(QDateTime::currentSecsSinceEpoch());
	bool bOk = false;
	QString LevelId = QInputDialog::getText(
		this,
		tr("新建地图"),
		tr("请输入地图ID:"),
		QLineEdit::Normal,
		DefaultName,
		&bOk);

	if (!bOk || LevelId.isEmpty())
	{
		return;
	}

	std::string ErrorMessage;
	if (!m_game_app_->CreateNewLevel(LevelId.toStdString(), &ErrorMessage))
	{
		QMessageBox::warning(
			this,
			tr("新建地图失败"),
			tr("无法创建新地图: %1").arg(QString::fromStdString(ErrorMessage)));
		return;
	}

	const GameplayConfig& Config = m_game_app_->GetGameplayConfig();
	m_editor_controls_syncing_ = true;
	if (m_map_id_edit_)
	{
		m_map_id_edit_->setText(LevelId);
	}
	m_editor_controls_syncing_ = false;
	PopulateGameplayConfigControls(Config);
	m_editor_dirty_ = true;
	UpdateEditorActionState();
	SetEditorStatus(tr("已创建并加载新地图: %1").arg(LevelId), false);
	RefreshScenePanels();
}

void MainWindow::OnOpenMapClicked()
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	QString DirPath = QString::fromStdString(m_game_app_->GetMapsDirectory().string());
	QString FilePath = QFileDialog::getOpenFileName(
		this,
		tr("打开地图"),
		DirPath,
		tr("地图文件 (*.json)"));

	if (FilePath.isEmpty())
	{
		return;
	}

	std::filesystem::path Path = FilePath.toStdString();
	std::string ErrorMessage;
	if (!m_game_app_->LoadLevelFromFile(Path, &ErrorMessage))
	{
		QMessageBox::warning(
			this,
			tr("打开地图失败"),
			tr("无法加载地图: %1").arg(QString::fromStdString(ErrorMessage)));
		return;
	}

	std::string LevelId = Path.filename().stem().string();
	const GameplayConfig& Config = m_game_app_->GetGameplayConfig();
	m_editor_controls_syncing_ = true;
	if (m_map_id_edit_)
	{
		m_map_id_edit_->setText(QString::fromStdString(LevelId));
	}
	m_editor_controls_syncing_ = false;
	PopulateGameplayConfigControls(Config);
	m_editor_dirty_ = true;
	UpdateEditorActionState();
	SetEditorStatus(tr("已加载地图: %1").arg(QString::fromStdString(LevelId)), false);
	RefreshScenePanels();
}

void MainWindow::OnSaveMapClicked()
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	std::string ErrorMessage;
	GameplayConfig Config;
	QString ErrorMsg;
	if (!BuildGameplayConfigFromControls(&Config, &ErrorMsg))
	{
		QMessageBox::warning(this, tr("配置错误"), ErrorMsg);
		return;
	}
	m_game_app_->SetGameplayConfig(Config);

	if (!m_game_app_->SaveCurrentLevelToDefault(&ErrorMessage))
	{
		QMessageBox::warning(
			this,
			tr("保存地图失败"),
			tr("无法保存地图: %1").arg(QString::fromStdString(ErrorMessage)));
		return;
	}

	SetEditorStatus(tr("地图已保存"), false);
}

void MainWindow::OnSaveAsMapClicked()
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	QString DirPath = QString::fromStdString(m_game_app_->GetMapsDirectory().string());
	QString FilePath = QFileDialog::getSaveFileName(
		this,
		tr("另存为"),
		DirPath,
		tr("地图文件 (*.json)"));

	if (FilePath.isEmpty())
	{
		return;
	}

	std::filesystem::path Path = FilePath.toStdString();
	if (!Path.extension().empty() && Path.extension() != ".json")
	{
		Path.replace_extension(".json");
	}
	else if (Path.extension().empty())
	{
		Path = Path.string() + ".json";
	}

	std::string ErrorMessage;
	GameplayConfig Config;
	QString ErrorMsg;
	if (!BuildGameplayConfigFromControls(&Config, &ErrorMsg))
	{
		QMessageBox::warning(this, tr("配置错误"), ErrorMsg);
		return;
	}
	m_game_app_->SetGameplayConfig(Config);

	if (!m_game_app_->SaveCurrentLevel(Path, &ErrorMessage))
	{
		QMessageBox::warning(
			this,
			tr("保存地图失败"),
			tr("无法保存地图: %1").arg(QString::fromStdString(ErrorMessage)));
		return;
	}

	SetEditorStatus(tr("地图已另存为: %1").arg(QString::fromStdString(Path.filename().stem().string())), false);
}

void MainWindow::OnImportAssetClicked()
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	const QString InitialDir = QString::fromStdString(m_game_app_->GetMapsDirectory().parent_path().string());
	const QString FilePath = QFileDialog::getOpenFileName(
		this,
		tr("导入资产"),
		InitialDir,
		tr("3D模型 (*.fbx *.obj *.gltf *.glb *.dae *.3ds *.blend);;所有文件 (*.*)"));

	if (FilePath.isEmpty())
	{
		return;
	}

	const QString ModelName = QFileInfo(FilePath).completeBaseName();
	ImportModelTransformDialog ImportDialog(ModelName, EImportModelDialogMode::ImportAsset, this);
	if (ImportDialog.exec() != QDialog::Accepted)
	{
		return;
	}

	const std::string ContentAssetPath = ImportDialog.GetContentAssetPath().toStdString();
	std::string SoftObjectPath;
	std::string ErrorMessage;
	if (!m_game_app_->ImportAssetFromSourceFile(
			std::filesystem::path(FilePath.toStdString()),
			ContentAssetPath,
			&SoftObjectPath,
			&ErrorMessage))
	{
		QMessageBox::warning(
			this,
			tr("导入失败"),
			tr("无法导入资产: %1").arg(QString::fromStdString(ErrorMessage)));
		return;
	}

	SetEditorStatus(
		tr("已导入资产: %1 → %2")
			.arg(ModelName)
			.arg(QString::fromStdString(SoftObjectPath)),
		false);
}

void MainWindow::OnLoadModelClicked()
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	const QString InitialDir = QString::fromStdString(GProjectContentDirectory.string());
	const QString FilePath = QFileDialog::getOpenFileName(
		this,
		tr("加载模型"),
		InitialDir,
		tr("uasset 资产 (*.uasset);;所有文件 (*.*)"));

	if (FilePath.isEmpty())
	{
		return;
	}

	const QString ModelName = QFileInfo(FilePath).completeBaseName();
	ImportModelTransformDialog LoadDialog(ModelName, EImportModelDialogMode::LoadModel, this);
	if (LoadDialog.exec() != QDialog::Accepted)
	{
		return;
	}

	const FActorTransform ActorTransform = LoadDialog.GetActorTransform();
	std::string ErrorMessage;
	if (!m_game_app_->LoadModelToActiveLevel(
			std::filesystem::path(FilePath.toStdString()),
			ActorTransform,
			&ErrorMessage))
	{
		QMessageBox::warning(
			this,
			tr("加载模型失败"),
			tr("无法加载模型: %1").arg(QString::fromStdString(ErrorMessage)));
		return;
	}

	SetEditorStatus(
		tr("已加载模型: %1（位置 %2,%3,%4）")
			.arg(ModelName)
			.arg(ActorTransform.Position.X, 0, 'f', 2)
			.arg(ActorTransform.Position.Y, 0, 'f', 2)
			.arg(ActorTransform.Position.Z, 0, 'f', 2),
		false);
	m_editor_dirty_ = true;
	RefreshScenePanels();
}

void MainWindow::OnExitClicked()
{
	close();
}

void MainWindow::OnDeleteSelectedActorTriggered()
{
	if (m_game_app_ == nullptr)
	{
		return;
	}

	QWidget* FocusWidget = QApplication::focusWidget();
	if (FocusWidget != nullptr)
	{
		if (qobject_cast<QLineEdit*>(FocusWidget) != nullptr
			|| qobject_cast<QAbstractSpinBox*>(FocusWidget) != nullptr)
		{
			return;
		}
	}

	if (m_game_app_->DeleteSelectedActor())
	{
		RefreshScenePanels();
	}
}

void MainWindow::UpdateRuntimeDisplay()
{
	if (!m_runtime_hint_label_)
	{
		return;
	}

	const QString TimeText =
		QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
	float CurrentFps = 0.0f;
	if (m_game_app_ != nullptr)
	{
		CurrentFps = m_game_app_->GetCurrentFramesPerSecond();
	}

	const QString FpsText =
		(CurrentFps > 0.0f)
			? QString::number(static_cast<double>(CurrentFps), 'f', 1)
			: tr("--");
	m_runtime_hint_label_->setText(
		tr("渲染中（Qt UI）\n时间: %1\n当前 FPS: %2").arg(TimeText, FpsText));
}
