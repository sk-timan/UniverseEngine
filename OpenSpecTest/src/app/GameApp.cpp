#include "app/GameApp.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <string>

#include <QApplication>

#include "data/EditorPerformanceStore.h"
#include "data/EditorPreferencesStore.h"
#include "ui/EditorStyle.h"
#include "ui/MainWindow.h"
#include "math/Math.h"
#include "editor/EditorActorBoundsDebug.h"
#include "editor/EditorPicking.h"
#include "editor/EditorTransformGizmo.h"
#include "editor/EditorViewMatrices.h"
#include "asset/AssetRegistry.h"
#include "asset/MeshImportFactory.h"
#include "asset/ProjectPaths.h"
#include "data/MeshImporter.h"
#include "world/Actor.h"
#include "world/Level.h"
#include "components/SceneComponent.h"

class UDemoObject : public UObject
{
public:
	UDemoObject(uint64_t InObjectId, std::string InObjectName, const UClass* InClass)
		: UObject(InObjectId, std::move(InObjectName), InClass)
	{
	}

	static const UClass& StaticClass()
	{
		static const UClass Class(
			"UDemoObject",
			&UObject::StaticClass(),
			[](uint64_t InObjectId, std::string InObjectName)
			{
				return std::make_unique<UDemoObject>(
					InObjectId,
					std::move(InObjectName),
					&UDemoObject::StaticClass());
			});
		return Class;
	}
};

namespace
{
constexpr float kCameraFocusMaxDurationSeconds = 0.3f;
constexpr float kCameraFocusMoveSpeed = 50.0f;
constexpr float kCameraFocusBoundsPadding = 1.35f;
constexpr float kCameraFocusMinDistance = 2.0f;
constexpr float kCameraFocusDefaultBoundsRadius = 1.0f;
constexpr float kCameraOrbitMouseSensitivity = 0.0025f;

float ComputeTravelDistance(const DirectX::XMFLOAT3& InStart, const DirectX::XMFLOAT3& InEnd)
{
	const float Dx = InEnd.x - InStart.x;
	const float Dy = InEnd.y - InStart.y;
	const float Dz = InEnd.z - InStart.z;
	return std::sqrt((Dx * Dx) + (Dy * Dy) + (Dz * Dz));
}

float ComputeFocusTransitionDuration(float InTravelDistance)
{
	if (InTravelDistance <= 1e-4f)
	{
		return 0.0f;
	}

	const float DurationAtConstantSpeed = InTravelDistance / kCameraFocusMoveSpeed;
	return std::min(DurationAtConstantSpeed, kCameraFocusMaxDurationSeconds);
}

FVector3 ComputeCameraForwardVector(const Dx12Renderer::CameraState& InCamera)
{
	const float ClampedPitch = std::clamp(InCamera.pitch, -1.4f, 1.4f);
	const float CosPitch = std::cos(ClampedPitch);
	return FVector3(
		CosPitch * std::cos(InCamera.yaw),
		CosPitch * std::sin(InCamera.yaw),
		std::sin(ClampedPitch));
}

float ComputeFocusDistanceFromBounds(const FEditorWorldAabb& InAabb, float InVerticalFovRadians)
{
	const FVector3 Extent = (InAabb.Max - InAabb.Min) * 0.5f;
	const float Radius = std::max(Extent.Length(), 0.25f);
	const float TanHalfFov = std::tan(InVerticalFovRadians * 0.5f);
	const float Distance = (Radius / std::max(TanHalfFov, 1e-4f)) * kCameraFocusBoundsPadding;
	return std::max(Distance, kCameraFocusMinDistance);
}

bool ComputeActorFocusCenter(const AActor* InActor, FVector3* OutFocusCenter)
{
	if (InActor == nullptr || OutFocusCenter == nullptr)
	{
		return false;
	}

	FEditorWorldAabb ActorAabb{};
	FEditorActorBoundsDebug::ComputeActorWorldAabb(InActor, &ActorAabb);
	if (ActorAabb.bIsValid)
	{
		*OutFocusCenter = (ActorAabb.Min + ActorAabb.Max) * 0.5f;
	}
	else
	{
		*OutFocusCenter = InActor->GetActorTransform().Position;
	}
	return true;
}

bool ComputeFocusPivotAndDistance(const AActor* InActor, FVector3* OutPivot, float* OutFocusDistance)
{
	if (InActor == nullptr || OutPivot == nullptr || OutFocusDistance == nullptr)
	{
		return false;
	}

	FVector3 FocusCenter{};
	if (!ComputeActorFocusCenter(InActor, &FocusCenter))
	{
		return false;
	}

	FEditorWorldAabb ActorAabb{};
	FEditorActorBoundsDebug::ComputeActorWorldAabb(InActor, &ActorAabb);

	float FocusDistance = kCameraFocusMinDistance;
	if (ActorAabb.bIsValid)
	{
		const float VerticalFovRadians = FMath::DegreesToRadians(FEditorViewMatrices::kCameraFovDegrees);
		FocusDistance = ComputeFocusDistanceFromBounds(ActorAabb, VerticalFovRadians);
	}
	else
	{
		FocusDistance = kCameraFocusDefaultBoundsRadius / std::tan(FMath::DegreesToRadians(FEditorViewMatrices::kCameraFovDegrees) * 0.5f);
		FocusDistance = std::max(FocusDistance * kCameraFocusBoundsPadding, kCameraFocusMinDistance);
	}

	*OutPivot = FocusCenter;
	*OutFocusDistance = FocusDistance;
	return true;
}

bool ComputeCameraOrbitPivotAndRadius(
	const Dx12Renderer::CameraState& InCamera,
	const AActor* InActor,
	FVector3* OutOrbitPivot,
	float* OutOrbitRadius)
{
	if (InActor == nullptr || OutOrbitPivot == nullptr || OutOrbitRadius == nullptr)
	{
		return false;
	}

	FVector3 FocusCenter{};
	if (!ComputeActorFocusCenter(InActor, &FocusCenter))
	{
		return false;
	}

	const FVector3 CameraPos(InCamera.position.x, InCamera.position.y, InCamera.position.z);
	const float ArmLength = std::max((CameraPos - FocusCenter).Length(), 0.5f);
	const FVector3 Forward = ComputeCameraForwardVector(InCamera);
	*OutOrbitPivot = CameraPos + (Forward * ArmLength);
	*OutOrbitRadius = ArmLength;
	return true;
}

void ApplyCameraOrbitPosition(
	Dx12Renderer::CameraState* InOutCamera,
	const FVector3& InPivot,
	float InOrbitRadius)
{
	if (InOutCamera == nullptr)
	{
		return;
	}

	const FVector3 Forward = ComputeCameraForwardVector(*InOutCamera);
	const FVector3 OrbitPosition = InPivot - (Forward * InOrbitRadius);
	InOutCamera->position.x = OrbitPosition.X;
	InOutCamera->position.y = OrbitPosition.Y;
	InOutCamera->position.z = OrbitPosition.Z;
}

bool ComputeFocusTargetPosition(
	const Dx12Renderer::CameraState& InCurrentCamera,
	const AActor* InActor,
	DirectX::XMFLOAT3* OutTargetPosition)
{
	if (InActor == nullptr || OutTargetPosition == nullptr)
	{
		return false;
	}

	FVector3 FocusCenter{};
	float FocusDistance = 0.0f;
	if (!ComputeFocusPivotAndDistance(InActor, &FocusCenter, &FocusDistance))
	{
		return false;
	}

	const FVector3 Forward = ComputeCameraForwardVector(InCurrentCamera);
	const FVector3 TargetPosition = FocusCenter - (Forward * FocusDistance);
	OutTargetPosition->x = TargetPosition.X;
	OutTargetPosition->y = TargetPosition.Y;
	OutTargetPosition->z = TargetPosition.Z;
	return true;
}
} // namespace

std::wstring ToWideString(const std::string& InText)
{
	if (InText.empty())
	{
		return std::wstring();
	}

	const int Required = MultiByteToWideChar(
		CP_UTF8, 0, InText.c_str(), static_cast<int>(InText.size()), nullptr, 0);
	if (Required <= 0)
	{
		return std::wstring(InText.begin(), InText.end());
	}

	std::wstring Converted(static_cast<size_t>(Required), L'\0');
	MultiByteToWideChar(
		CP_UTF8, 0, InText.c_str(), static_cast<int>(InText.size()), Converted.data(), Required);
	return Converted;
}


GameApp::GameApp()
{
	m_transform_gizmo_.SetMode(EGizmoMode::Translate);
	m_gizmo_mode_ = EGizmoMode::Translate;
}
GameApp::~GameApp() = default;

int GameApp::Run(HINSTANCE InInstance, int InShowCommand)
{
	(void)InInstance;
	int ArgC = 1;
	char AppName[] = "OpenSpecTest";
	char* ArgV[] = {AppName, nullptr};
	QApplication QtApp(ArgC, ArgV);
	EditorStyle::ApplyApplicationStyle(&QtApp);

	std::wstring InitError;
	if (!InitializeDataDrivenResources(&InitError))
	{
		const std::wstring Message = L"Data-driven resource initialization failed.\n" + InitError;
		MessageBoxW(nullptr, Message.c_str(), L"OpenSpecTest", MB_ICONERROR | MB_OK);
		return -1;
	}

	m_main_window_ = std::make_unique<MainWindow>(this);
	m_main_window_->resize(1480, 840);
	m_main_window_->show();
	(void)InShowCommand;
	m_hwnd_ = m_main_window_->GetRenderWindowHandle();

	const UINT RenderWidth = static_cast<UINT>(std::max(1, m_main_window_->GetRenderWidth()));
	const UINT RenderHeight = static_cast<UINT>(std::max(1, m_main_window_->GetRenderHeight()));
	if (!m_renderer_.Initialize(m_hwnd_, RenderWidth, RenderHeight))
	{
		MessageBoxW(nullptr, L"DX12 initialization failed.", L"OpenSpecTest", MB_ICONERROR | MB_OK);
		return -1;
	}
	m_renderer_initialized_ = true;

	std::string ApplyPrefsError;
	if (!ApplyEditorPreferences(m_editor_preferences_, &ApplyPrefsError))
	{
		const std::wstring Message = L"Failed to apply editor preferences.\n" + ToWideString(ApplyPrefsError);
		MessageBoxW(nullptr, Message.c_str(), L"OpenSpecTest", MB_ICONWARNING | MB_OK);
	}

	// 默认俯视原点：位于 +X/+Y/+Z 象限，朝向 (-X,-Y,-Z)，与编辑器网格/坐标轴预览一致。
	m_camera_.position = {20.0f, 10.0f, 8.0f};
	m_camera_.yaw = FMath::DegreesToRadians(-155.0f);
	m_camera_.pitch = FMath::DegreesToRadians(-20.0f);
	m_last_frame_time_ = std::chrono::steady_clock::now();
	m_next_device_recovery_attempt_time_ = m_last_frame_time_;
	m_main_window_->StartFrameLoop();

	const int ExitCode = QtApp.exec();
	Shutdown();
	return ExitCode;
}

void GameApp::Tick()
{
	if (!m_renderer_initialized_ || m_is_minimized_)
	{
		return;
	}

	const auto Now = std::chrono::steady_clock::now();
	if (m_renderer_.IsDeviceLost())
	{
		if (Now >= m_next_device_recovery_attempt_time_)
		{
			const bool bRecovered = m_renderer_.TryRecoverDevice();
			if (bRecovered)
			{
				m_has_device_lost_reported_ = false;
				m_last_frame_time_ = Now;
			}
			else
			{
				m_has_device_lost_reported_ = true;
				m_next_device_recovery_attempt_time_ = Now + std::chrono::milliseconds(300);
			}
		}
		return;
	}

	if (m_has_pending_resize_)
	{
		if (Now >= m_next_resize_attempt_time_ &&
			m_renderer_.Resize(m_pending_resize_width_, m_pending_resize_height_))
		{
			m_has_pending_resize_ = false;
			m_last_frame_time_ = Now;
			return;
		}
		else if (Now >= m_next_resize_attempt_time_)
		{
			m_next_resize_attempt_time_ = Now + std::chrono::milliseconds(80);
		}
		return;
	}

	const float DeltaSeconds =
		std::chrono::duration<float>(Now - m_last_frame_time_).count();
	m_last_frame_time_ = Now;
	if (DeltaSeconds > 0.0f)
	{
		m_fps_sample_accumulator_seconds_ += DeltaSeconds;
		++m_fps_sample_frame_count_;
		if (m_fps_sample_accumulator_seconds_ >= 0.25f)
		{
			m_current_frames_per_second_ =
				static_cast<float>(m_fps_sample_frame_count_) / m_fps_sample_accumulator_seconds_;
			m_fps_sample_accumulator_seconds_ = 0.0f;
			m_fps_sample_frame_count_ = 0;
		}
	}
	UpdateEditorKeyboard();
	TickCameraFocusTransition(DeltaSeconds);
	UpdateInput(DeltaSeconds);
	TickEditorInteraction(DeltaSeconds);
	if (m_world_ != nullptr)
	{
		m_world_->Tick(DeltaSeconds);
		m_world_->Render(&m_renderer_);
	}
	m_renderer_.SetEditorGizmoMesh(m_gizmo_mesh_vertices_, true);
	m_renderer_.Render(m_camera_);
}

float GameApp::GetCurrentFramesPerSecond() const
{
	return m_current_frames_per_second_;
}

void GameApp::OnResize(UINT InWidth, UINT InHeight)
{
	if (!m_renderer_initialized_)
	{
		return;
	}
	if (InWidth == 0 || InHeight == 0)
	{
		m_is_minimized_ = true;
		m_has_pending_resize_ = false;
		return;
	}

	m_is_minimized_ = false;
	m_pending_resize_width_ = InWidth;
	m_pending_resize_height_ = InHeight;
	m_has_pending_resize_ = true;
	m_next_resize_attempt_time_ = std::chrono::steady_clock::now();
	if (m_is_mouse_look_active_)
	{
		UpdateMouseLookBounds();
		SetCursorPos(m_mouse_look_center_screen_position_.x, m_mouse_look_center_screen_position_.y);
	}
}

void GameApp::OnFocusGained()
{
	m_viewport_has_focus_ = true;
}

void GameApp::OnFocusLost()
{
	m_viewport_has_focus_ = false;
	EndMouseLook(false);
	EndCameraOrbit();
	m_camera_velocity_.position = {0.0f, 0.0f, 0.0f};
}

void GameApp::SetMouseLookActive(bool bIsActive, bool bRestoreCursor)
{
	if (bIsActive)
	{
		BeginMouseLook();
	}
	else
	{
		EndMouseLook(bRestoreCursor);
	}
}

float GameApp::GetCameraMoveSpeed() const
{
	return m_camera_move_speed_;
}

float GameApp::GetCameraSpeedScalar() const
{
	return m_camera_speed_scalar_;
}

void GameApp::SetCameraMoveSpeed(float InSpeed)
{
	m_camera_move_speed_ = std::clamp(InSpeed, 1.0f, 32.0f);
	m_editor_preferences_.CameraMoveSpeed = m_camera_move_speed_;
}

void GameApp::SetCameraSpeedScalar(float InScalar)
{
	m_camera_speed_scalar_ = std::clamp(InScalar, 0.1f, 10.0f);
	m_editor_preferences_.CameraSpeedScalar = m_camera_speed_scalar_;
}

const FEditorPreferences& GameApp::GetEditorPreferences() const
{
	return m_editor_preferences_;
}

std::filesystem::path GameApp::GetEditorPreferencesIniPath() const
{
	return m_editor_preferences_ini_path_;
}

bool GameApp::LoadEditorPreferences(std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		OutErrorMessage->clear();
	}

	if (m_editor_preferences_ini_path_.empty())
	{
		if (m_executable_dir_.empty())
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Executable directory is not initialized.";
			}
			return false;
		}
		m_editor_preferences_ini_path_ = EditorPreferencesStore::GetDefaultIniPath(m_executable_dir_);
	}

	FEditorPreferences LoadedPreferences{};
	if (!EditorPreferencesStore::LoadFromFile(m_editor_preferences_ini_path_, &LoadedPreferences, OutErrorMessage))
	{
		return false;
	}

	m_editor_preferences_ = LoadedPreferences;
	m_camera_move_speed_ = m_editor_preferences_.CameraMoveSpeed;
	m_camera_speed_scalar_ = m_editor_preferences_.CameraSpeedScalar;

	if (!std::filesystem::exists(m_editor_preferences_ini_path_))
	{
		return SaveEditorPreferences(OutErrorMessage);
	}

	return true;
}

bool GameApp::SaveEditorPreferences(std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		OutErrorMessage->clear();
	}

	m_editor_preferences_.CameraMoveSpeed = m_camera_move_speed_;
	m_editor_preferences_.CameraSpeedScalar = m_camera_speed_scalar_;

	if (m_renderer_initialized_)
	{
		m_editor_preferences_.NearClipPlane = m_renderer_.GetNearClipPlane();
		m_editor_preferences_.FarClipPlane = m_renderer_.GetFarClipPlane();
	}

	return EditorPreferencesStore::SaveToFile(m_editor_preferences_ini_path_, m_editor_preferences_, OutErrorMessage);
}

const FEditorPerformanceSettings& GameApp::GetEditorPerformanceSettings() const
{
	return m_performance_settings_;
}

std::filesystem::path GameApp::GetEditorPerformanceIniPath() const
{
	return m_performance_settings_ini_path_;
}

bool GameApp::LoadEditorPerformanceSettings(std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		OutErrorMessage->clear();
	}

	if (m_performance_settings_ini_path_.empty())
	{
		if (m_executable_dir_.empty())
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = "Executable directory is not initialized.";
			}
			return false;
		}
		m_performance_settings_ini_path_ = EditorPerformanceStore::GetDefaultIniPath(m_executable_dir_);
	}

	FEditorPerformanceSettings LoadedSettings{};
	if (!EditorPerformanceStore::LoadFromFile(m_performance_settings_ini_path_, &LoadedSettings, OutErrorMessage))
	{
		return false;
	}

	m_performance_settings_ = LoadedSettings;

	if (!std::filesystem::exists(m_performance_settings_ini_path_))
	{
		return SaveEditorPerformanceSettings(OutErrorMessage);
	}

	return true;
}

bool GameApp::SaveEditorPerformanceSettings(std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		OutErrorMessage->clear();
	}

	return EditorPerformanceStore::SaveToFile(
		m_performance_settings_ini_path_,
		m_performance_settings_,
		OutErrorMessage);
}

bool GameApp::ApplyEditorPerformanceSettings(
	const FEditorPerformanceSettings& InSettings,
	std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		OutErrorMessage->clear();
	}

	if (!EditorPerformanceStore::Validate(InSettings, OutErrorMessage))
	{
		return false;
	}

	if (InSettings.TriangleBvhSplitMethod != m_performance_settings_.TriangleBvhSplitMethod)
	{
		FEditorPicking::InvalidateTriangleBvhCache();
	}

	m_performance_settings_ = InSettings;
	return true;
}

bool GameApp::ApplyEditorPreferences(const FEditorPreferences& InPreferences, std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		OutErrorMessage->clear();
	}

	if (!EditorPreferencesStore::Validate(InPreferences, OutErrorMessage))
	{
		return false;
	}

	m_editor_preferences_ = InPreferences;
	SetCameraMoveSpeed(m_editor_preferences_.CameraMoveSpeed);
	SetCameraSpeedScalar(m_editor_preferences_.CameraSpeedScalar);

	if (m_renderer_initialized_)
	{
		m_renderer_.SetCameraClipDistances(
			m_editor_preferences_.NearClipPlane,
			m_editor_preferences_.FarClipPlane);
	}

	return true;
}

const GameplayConfig& GameApp::GetGameplayConfig() const
{
	return m_gameplay_config_;
}

std::filesystem::path GameApp::GetMapsDirectory() const
{
	return m_maps_directory_;
}

void GameApp::SetGameplayConfig(const GameplayConfig& InConfig)
{
	m_gameplay_config_ = InConfig;
}

bool GameApp::LoadMapById(const std::string& InMapId, std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		OutErrorMessage->clear();
	}
	if (m_world_ == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "World is not initialized.";
		}
		return false;
	}

	std::string ResolveError;
	if (!EnsureLevelDefinitionForMapId(InMapId, &ResolveError))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = ResolveError;
		}
		return false;
	}

	std::string LevelLoadError;
	if (!m_world_->LoadLevel(InMapId, &LevelLoadError))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Failed to activate map_id '" + InMapId + "': " + LevelLoadError;
		}
		return false;
	}

	SyncRendererLevelInput();
	BumpSceneRevision();
	m_selected_actor_object_id_ = 0;
	return true;
}

bool GameApp::CreateNewDefaultLevel(std::string* OutCreatedMapId, std::string* OutErrorMessage)
{
	if (OutCreatedMapId != nullptr)
	{
		OutCreatedMapId->clear();
	}
	if (OutErrorMessage != nullptr)
	{
		OutErrorMessage->clear();
	}
	if (m_world_ == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "World is not initialized.";
		}
		return false;
	}

	std::string NewMapId;
	do
	{
		NewMapId = "runtime_new_level_" + std::to_string(m_runtime_new_level_serial_);
		++m_runtime_new_level_serial_;
	} while (m_world_->HasLevelDefinition(NewMapId));

	std::filesystem::path SavePath = m_maps_directory_ / (NewMapId + ".json");
	std::string RegisterError;
	if (!m_world_->RegisterLevelDefinitionWithSavePath(
			FLevelDefinition{NewMapId, std::filesystem::path()},
			SavePath,
			&RegisterError))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = RegisterError;
		}
		return false;
	}
	if (!LoadMapById(NewMapId, OutErrorMessage))
	{
		return false;
	}

	if (OutCreatedMapId != nullptr)
	{
		*OutCreatedMapId = NewMapId;
	}

	m_current_level_save_path_ = SavePath;

	return true;
}

bool GameApp::CreateNewLevel(const std::string& InLevelId, std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		OutErrorMessage->clear();
	}
	if (m_world_ == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "World is not initialized.";
		}
		return false;
	}

	if (InLevelId.empty())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "LevelId must not be empty.";
		}
		return false;
	}

	std::string RegisterError;
	std::filesystem::path SavePath = m_maps_directory_ / (InLevelId + ".json");
	if (!m_world_->RegisterLevelDefinitionWithSavePath(
			FLevelDefinition{InLevelId, std::filesystem::path()},
			SavePath,
			&RegisterError))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = RegisterError;
		}
		return false;
	}

	if (!LoadMapById(InLevelId, OutErrorMessage))
	{
		return false;
	}

	m_current_level_save_path_ = SavePath;
	return true;
}

bool GameApp::LoadLevelFromFile(const std::filesystem::path& InFilePath, std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		OutErrorMessage->clear();
	}
	if (m_world_ == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "World is not initialized.";
		}
		return false;
	}

	if (!std::filesystem::exists(InFilePath))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "File does not exist: " + InFilePath.string();
		}
		return false;
	}

	std::filesystem::path FileName = InFilePath.filename();
	std::string LevelId = FileName.stem().string();

	std::string RegisterError;
	if (!m_world_->RegisterLevelDefinitionWithSavePath(
			FLevelDefinition{LevelId, std::filesystem::path()},
			InFilePath,
			&RegisterError))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = RegisterError;
		}
		return false;
	}

	if (!LoadMapById(LevelId, OutErrorMessage))
	{
		return false;
	}

	m_current_level_save_path_ = InFilePath;

	ULevel* ActiveLevel = m_world_->GetActiveLevel();
	if (ActiveLevel != nullptr)
	{
		if (!ActiveLevel->LoadFromFile(InFilePath, OutErrorMessage))
		{
			return false;
		}
		const GameplayConfig& LoadedConfig = ActiveLevel->GetGameplayConfig();
		m_gameplay_config_ = LoadedConfig;
	}

	BumpSceneRevision();
	m_selected_actor_object_id_ = 0;
	RefreshActiveLevelRender();
	return true;
}

bool GameApp::SaveCurrentLevel(const std::filesystem::path& InFilePath, std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		OutErrorMessage->clear();
	}
	if (m_world_ == nullptr || !m_world_->HasActiveLevel())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "No active level to save.";
		}
		return false;
	}

	ULevel* ActiveLevel = m_world_->GetActiveLevel();
	if (ActiveLevel == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Active level is null.";
		}
		return false;
	}

	ActiveLevel->SetGameplayConfig(m_gameplay_config_);

	if (!ActiveLevel->SaveToFile(InFilePath, OutErrorMessage))
	{
		return false;
	}

	m_current_level_save_path_ = InFilePath;
	return true;
}

bool GameApp::ImportAssetFromSourceFile(
	const std::filesystem::path& InSourceFile,
	const std::string& InContentAssetPath,
	std::string* OutSoftObjectPath,
	std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		OutErrorMessage->clear();
	}
	if (OutSoftObjectPath != nullptr)
	{
		OutSoftObjectPath->clear();
	}
	if (InSourceFile.empty() || !std::filesystem::exists(InSourceFile))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Source model file does not exist: " + InSourceFile.string();
		}
		return false;
	}

	const std::filesystem::path CanonicalPath = std::filesystem::weakly_canonical(InSourceFile);
	const std::string AssetPath = InContentAssetPath.empty()
		? std::string("Meshes/Imported/") + CanonicalPath.stem().string()
		: InContentAssetPath;
	const std::string ObjectName = std::filesystem::path(AssetPath).filename().string();
	const bool bIsSkeletal = MeshImporter::ProbeIsSkeletalModel(CanonicalPath);

	FMeshImportRequest ImportRequest;
	ImportRequest.SourceFile = CanonicalPath;
	ImportRequest.AssetPath = AssetPath;
	ImportRequest.ObjectName = ObjectName.empty() ? CanonicalPath.stem().string() : ObjectName;

	std::string SoftPath;
	if (bIsSkeletal)
	{
		if (UMeshImportFactory::ImportSkeletalMeshAndSave(ImportRequest, &SoftPath, OutErrorMessage) == nullptr)
		{
			return false;
		}
	}
	else
	{
		if (UMeshImportFactory::ImportStaticMeshAndSave(ImportRequest, &SoftPath, OutErrorMessage) == nullptr)
		{
			return false;
		}
	}

	if (OutSoftObjectPath != nullptr)
	{
		*OutSoftObjectPath = SoftPath;
	}
	return true;
}

bool GameApp::LoadModelToActiveLevel(
	const std::filesystem::path& InUAssetFilePath,
	const FActorTransform& InActorTransform,
	std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		OutErrorMessage->clear();
	}
	if (m_world_ == nullptr || !m_world_->HasActiveLevel())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "No active level loaded.";
		}
		return false;
	}

	ULevel* ActiveLevel = m_world_->GetActiveLevel();
	if (ActiveLevel == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Active level is null.";
		}
		return false;
	}

	std::string SoftPath;
	if (!TryBuildSoftPathFromUAssetFile(InUAssetFilePath, &SoftPath, OutErrorMessage))
	{
		return false;
	}

	AActor* LoadedActor = nullptr;
	if (!ActiveLevel->SpawnModelFromSoftPath(SoftPath, InActorTransform, &LoadedActor, OutErrorMessage))
	{
		return false;
	}

	if (LoadedActor != nullptr)
	{
		SelectActor(LoadedActor->GetObjectId());
	}
	BumpSceneRevision();
	RefreshActiveLevelRender();
	return true;
}

void GameApp::RefreshActiveLevelRender(bool bInvalidateSceneCache)
{
	if (!m_renderer_initialized_ || m_world_ == nullptr || !m_world_->HasActiveLevel())
	{
		return;
	}

	if (bInvalidateSceneCache)
	{
		m_renderer_.InvalidateSceneRenderCache();
	}
	m_world_->Render(&m_renderer_);
	m_renderer_.Render(m_camera_);
}

bool GameApp::SaveCurrentLevelToDefault(std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		OutErrorMessage->clear();
	}

	if (m_current_level_save_path_.empty())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "No save path configured for current level.";
		}
		return false;
	}

	return SaveCurrentLevel(m_current_level_save_path_, OutErrorMessage);
}

bool GameApp::ApplyGameplayConfig(const GameplayConfig& InConfig, std::string* OutErrorMessage)
{
	if (m_gameplay_config_path_.empty())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Gameplay config path is not initialized.";
		}
		return false;
	}
	if (m_world_ != nullptr)
	{
		std::string ResolveError;
		if (!EnsureLevelDefinitionForMapId(InConfig.map_id, &ResolveError))
		{
			if (OutErrorMessage != nullptr)
			{
				*OutErrorMessage = ResolveError;
			}
			return false;
		}
	}
	if (!GameplayConfigStore::SaveToFile(m_gameplay_config_path_, InConfig, OutErrorMessage))
	{
		return false;
	}
	if (m_world_ != nullptr && !LoadMapById(InConfig.map_id, OutErrorMessage))
	{
		return false;
	}
	m_gameplay_config_ = InConfig;
	return true;
}

void GameApp::Shutdown()
{
	EndMouseLook(false);
	if (m_world_ != nullptr)
	{
		(void)m_world_->UnloadLevel(nullptr);
		m_world_.reset();
	}
	m_main_window_.reset();
}

bool GameApp::InitializeDataDrivenResources(std::wstring* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		OutErrorMessage->clear();
	}

	wchar_t ModulePath[MAX_PATH]{};
	const DWORD ModulePathLength = GetModuleFileNameW(nullptr, ModulePath, MAX_PATH);
	if (ModulePathLength == 0 || ModulePathLength == MAX_PATH)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = L"Failed to resolve executable path.";
		}
		return false;
	}

	const std::filesystem::path ExecutableDir = std::filesystem::path(ModulePath).parent_path();
	m_executable_dir_ = ExecutableDir;
	m_editor_preferences_ini_path_ = EditorPreferencesStore::GetDefaultIniPath(ExecutableDir);
	m_performance_settings_ini_path_ = EditorPerformanceStore::GetDefaultIniPath(ExecutableDir);
	m_gameplay_config_path_ = ExecutableDir / "data" / "config" / "gameplay.json";
	m_maps_directory_ = ExecutableDir / "data" / "maps";

	if (!std::filesystem::exists(m_maps_directory_))
	{
		std::filesystem::create_directories(m_maps_directory_);
	}

	std::error_code ContentDirError;
	std::filesystem::create_directories(GProjectContentDirectory, ContentDirError);
	FAssetRegistry::Get().ScanContentDirectory();

	std::string LoaderError;
	if (!m_resource_loader_.Initialize(ExecutableDir / "data", &LoaderError))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = ToWideString(LoaderError);
		}
		return false;
	}

	if (!m_resource_loader_.LoadGameplayConfig(
			"config/gameplay.json", &m_gameplay_config_, &LoaderError))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = ToWideString(LoaderError);
		}
		return false;
	}

	if (!m_resource_loader_.LoadFishSpeciesDefs(
			"fish/fish_species.csv", &m_fish_species_defs_, &LoaderError))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = ToWideString(LoaderError);
		}
		return false;
	}

	if (!m_resource_loader_.LoadModelAsset(
			"models/glTF2/2CylinderEngine-glTF-Binary/2CylinderEngine.glb", &m_rod_model_asset_, &LoaderError))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = ToWideString(LoaderError);
		}
		return false;
	}

	std::filesystem::path ResolvedMapModelPath;
	std::string ResolveError;
	if (ResolveMapModelPathByMapId(m_gameplay_config_.map_id, &ResolvedMapModelPath, &ResolveError))
	{
		m_default_map_model_path_ = ResolvedMapModelPath;
	}
	else
	{
		m_default_map_model_path_.clear();
	}

	m_world_ = std::make_unique<UWorld>(&m_object_registry_);
	std::string WorldError;
	if (!m_world_->RegisterLevelDefinition(
			FLevelDefinition{m_gameplay_config_.map_id, ResolvedMapModelPath},
			&WorldError))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = ToWideString(WorldError);
		}
		return false;
	}
	if (!m_world_->LoadLevel(m_gameplay_config_.map_id, &WorldError))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = ToWideString(WorldError);
		}
		return false;
	}
	SyncRendererLevelInput();

	std::string EditorPrefsError;
	if (!LoadEditorPreferences(&EditorPrefsError))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = ToWideString(EditorPrefsError);
		}
		return false;
	}

	std::string PerformanceSettingsError;
	if (!LoadEditorPerformanceSettings(&PerformanceSettingsError))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = ToWideString(PerformanceSettingsError);
		}
		return false;
	}

	return m_gameplay_config_.version > 0 &&
		!m_gameplay_config_.map_id.empty() &&
		!m_fish_species_defs_.empty() &&
		m_rod_model_asset_.node_count > 0;
}

bool GameApp::EnsureLevelDefinitionForMapId(const std::string& InMapId, std::string* OutErrorMessage)
{
	if (OutErrorMessage != nullptr)
	{
		OutErrorMessage->clear();
	}
	if (m_world_ == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "World is not initialized.";
		}
		return false;
	}
	if (InMapId.empty())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Unknown map_id for world loading: empty map_id";
		}
		return false;
	}
	if (m_world_->HasLevelDefinition(InMapId))
	{
		return true;
	}

	std::filesystem::path MapModelPath;
	std::string ResolveError;
	if (!ResolveMapModelPathByMapId(InMapId, &MapModelPath, &ResolveError))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = ResolveError;
		}
		return false;
	}

	std::string RegisterError;
	if (!m_world_->RegisterLevelDefinition(
			FLevelDefinition{InMapId, MapModelPath},
			&RegisterError))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = RegisterError;
		}
		return false;
	}
	return true;
}

bool GameApp::ResolveMapModelPathByMapId(
	const std::string& InMapId,
	std::filesystem::path* OutMapModelPath,
	std::string* OutErrorMessage) const
{
	if (OutErrorMessage != nullptr)
	{
		OutErrorMessage->clear();
	}
	if (OutMapModelPath == nullptr)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "OutMapModelPath is null.";
		}
		return false;
	}

	if (InMapId.empty())
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = "Unknown map_id for world loading: empty map_id";
		}
		return false;
	}

	if (InMapId == m_gameplay_config_.map_id && !m_default_map_model_path_.empty())
	{
		*OutMapModelPath = m_default_map_model_path_;
		return true;
	}

	const std::filesystem::path CandidateA =
		m_executable_dir_ / "data" / "model" / "fbx" / (InMapId + ".fbx");
	const std::filesystem::path CandidateB =
		m_executable_dir_ / "data" / "models" / "FBX" / (InMapId + ".fbx");
	const std::filesystem::path CandidateC =
		m_executable_dir_ / "data" / "models" / "fbx" / (InMapId + ".fbx");
	if (std::filesystem::exists(CandidateA))
	{
		*OutMapModelPath = CandidateA;
		return true;
	}
	if (std::filesystem::exists(CandidateB))
	{
		*OutMapModelPath = CandidateB;
		return true;
	}
	if (std::filesystem::exists(CandidateC))
	{
		*OutMapModelPath = CandidateC;
		return true;
	}

	if (OutErrorMessage != nullptr)
	{
		*OutErrorMessage = "Unknown map_id for world loading: " + InMapId;
	}
	return false;
}

void GameApp::SyncRendererLevelInput()
{
	m_renderer_.SetMapModelPath(std::filesystem::path());
}

void GameApp::UpdateInput(float DeltaSeconds)
{
	const float Dt = std::clamp(DeltaSeconds, 0.0f, 0.05f);
	if (Dt <= 0.0f)
	{
		return;
	}
	if (m_b_camera_focus_active_ || m_b_camera_orbit_active_)
	{
		m_camera_velocity_.position = {0.0f, 0.0f, 0.0f};
		return;
	}
	if (!m_viewport_has_focus_)
	{
		m_camera_velocity_.position = {0.0f, 0.0f, 0.0f};
		return;
	}

	const bool bSprinting = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
	const float BaseMoveSpeed = m_camera_move_speed_ * m_camera_speed_scalar_;
	constexpr float kSprintSpeedMultiplier = 1.9f;
	const float MoveSpeed = bSprinting ? (BaseMoveSpeed * kSprintSpeedMultiplier) : BaseMoveSpeed;
	const float TurnSpeed = 1.8f;
	const float MouseSensitivity = 0.0025f;
	const float SmoothingStrength = 12.0f;

	const float YawDelta =
		(((GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0) ? 1.0f : 0.0f) -
		(((GetAsyncKeyState(VK_LEFT) & 0x8000) != 0) ? 1.0f : 0.0f);
	const float PitchDelta =
		(((GetAsyncKeyState(VK_UP) & 0x8000) != 0) ? 1.0f : 0.0f) -
		(((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0) ? 1.0f : 0.0f);
	m_camera_.yaw += YawDelta * TurnSpeed * Dt;
	m_camera_.pitch += PitchDelta * TurnSpeed * Dt;

	if (m_is_mouse_look_active_)
	{
		POINT CursorScreen{};
		if (GetCursorPos(&CursorScreen))
		{
			const float MouseDx =
				static_cast<float>(CursorScreen.x - m_mouse_look_center_screen_position_.x);
			const float MouseDy =
				static_cast<float>(CursorScreen.y - m_mouse_look_center_screen_position_.y);

			m_camera_.yaw += MouseDx * MouseSensitivity;
			m_camera_.pitch -= MouseDy * MouseSensitivity;
			SetCursorPos(
				m_mouse_look_center_screen_position_.x,
				m_mouse_look_center_screen_position_.y);
		}
	}

	m_camera_.pitch = std::clamp(m_camera_.pitch, -1.4f, 1.4f);

	const float CosPitch = std::cos(m_camera_.pitch);
	const float SinPitch = std::sin(m_camera_.pitch);
	const float SinYaw = std::sin(m_camera_.yaw);
	const float CosYaw = std::cos(m_camera_.yaw);

	const float ForwardX = CosPitch * CosYaw;
	const float ForwardY = CosPitch * SinYaw;
	const float ForwardZ = SinPitch;
	const float ForwardLength = std::sqrt(
		(ForwardX * ForwardX) + (ForwardY * ForwardY) + (ForwardZ * ForwardZ));
	const float InvForwardLength = (ForwardLength > 0.0001f) ? (1.0f / ForwardLength) : 1.0f;

	const float Fx = ForwardX * InvForwardLength;
	const float Fy = ForwardY * InvForwardLength;
	const float Fz = ForwardZ * InvForwardLength;

	const float Rx = -Fy;
	const float Ry = Fx;
	const float RightLength = std::sqrt((Rx * Rx) + (Ry * Ry));
	const float InvRightLength = (RightLength > 0.0001f) ? (1.0f / RightLength) : 1.0f;
	const float RightX = Rx * InvRightLength;
	const float RightY = Ry * InvRightLength;

	float MoveForward = 0.0f;
	float MoveRight = 0.0f;
	float MoveUp = 0.0f;
	if (m_is_mouse_look_active_)
	{
		MoveForward =
			(((GetAsyncKeyState('W') & 0x8000) != 0) ? 1.0f : 0.0f) -
			(((GetAsyncKeyState('S') & 0x8000) != 0) ? 1.0f : 0.0f);
		MoveRight =
			(((GetAsyncKeyState('D') & 0x8000) != 0) ? 1.0f : 0.0f) -
			(((GetAsyncKeyState('A') & 0x8000) != 0) ? 1.0f : 0.0f);
		MoveUp =
			(((GetAsyncKeyState('E') & 0x8000) != 0) ? 1.0f : 0.0f) -
			(((GetAsyncKeyState('Q') & 0x8000) != 0) ? 1.0f : 0.0f);
	}
	else
	{
		m_camera_velocity_.position = {0.0f, 0.0f, 0.0f};
	}

	const float DesiredVx = ((Fx * MoveForward) + (RightX * MoveRight)) * MoveSpeed;
	const float DesiredVy = ((Fy * MoveForward) + (RightY * MoveRight)) * MoveSpeed;
	const float DesiredVz = ((Fz * MoveForward) + MoveUp) * MoveSpeed;

	const float Blend = 1.0f - std::exp(-SmoothingStrength * Dt);
	m_camera_velocity_.position.x += (DesiredVx - m_camera_velocity_.position.x) * Blend;
	m_camera_velocity_.position.y += (DesiredVy - m_camera_velocity_.position.y) * Blend;
	m_camera_velocity_.position.z += (DesiredVz - m_camera_velocity_.position.z) * Blend;

	m_camera_.position.x += m_camera_velocity_.position.x * Dt;
	m_camera_.position.y += m_camera_velocity_.position.y * Dt;
	m_camera_.position.z += m_camera_velocity_.position.z * Dt;
}

void GameApp::UpdateMouseLookBounds()
{
	RECT ClientRect{};
	if (!GetClientRect(m_hwnd_, &ClientRect))
	{
		return;
	}

	POINT TopLeft{ClientRect.left, ClientRect.top};
	POINT BottomRight{ClientRect.right, ClientRect.bottom};
	if (!ClientToScreen(m_hwnd_, &TopLeft) || !ClientToScreen(m_hwnd_, &BottomRight))
	{
		return;
	}

	RECT ClipRect{TopLeft.x, TopLeft.y, BottomRight.x, BottomRight.y};
	ClipCursor(&ClipRect);

	m_mouse_look_center_screen_position_.x = (ClipRect.left + ClipRect.right) / 2;
	m_mouse_look_center_screen_position_.y = (ClipRect.top + ClipRect.bottom) / 2;
}

void GameApp::RunObjectSystemDemo()
{
	if (m_has_run_object_system_demo_)
	{
		return;
	}

	m_has_run_object_system_demo_ = true;
	std::string ErrorMessage;
	UObject* Root = m_object_registry_.NewObject(
		UDemoObject::StaticClass(),
		FNewObjectParams{"DemoRoot", true},
		&ErrorMessage);
	UObject* ChildA = m_object_registry_.NewObject(
		UDemoObject::StaticClass(),
		FNewObjectParams{"DemoChildA", false},
		&ErrorMessage);
	UObject* ChildB = m_object_registry_.NewObject(
		UDemoObject::StaticClass(),
		FNewObjectParams{"DemoChildB", false},
		&ErrorMessage);
	if (Root == nullptr || ChildA == nullptr || ChildB == nullptr)
	{
		OutputDebugStringA("Object system demo failed to create objects.\n");
		return;
	}

	Root->AddReferencedObject(ChildA);
	ChildA->AddReferencedObject(ChildB);
	const FGarbageCollectionResult FirstCollect = m_object_registry_.CollectGarbage();
	if (FirstCollect.DestroyedCount != 0)
	{
		OutputDebugStringA("Object system demo unexpected sweep in rooted graph.\n");
	}

	m_object_registry_.RemoveFromRoot(Root->GetObjectId());
	const FGarbageCollectionResult SecondCollect = m_object_registry_.CollectGarbage();
	if (SecondCollect.DestroyedCount != 3)
	{
		OutputDebugStringA("Object system demo expected to sweep 3 objects.\n");
	}

	UObject* CycleA = m_object_registry_.NewObject(
		UDemoObject::StaticClass(),
		FNewObjectParams{"DemoCycleA", false},
		&ErrorMessage);
	UObject* CycleB = m_object_registry_.NewObject(
		UDemoObject::StaticClass(),
		FNewObjectParams{"DemoCycleB", false},
		&ErrorMessage);
	if (CycleA == nullptr || CycleB == nullptr)
	{
		OutputDebugStringA("Object system demo failed to create cycle objects.\n");
		return;
	}

	CycleA->AddReferencedObject(CycleB);
	CycleB->AddReferencedObject(CycleA);
	const FGarbageCollectionResult CycleCollect = m_object_registry_.CollectGarbage();
	if (CycleCollect.DestroyedCount != 2)
	{
		OutputDebugStringA("Object system demo expected to sweep 2 cycle objects.\n");
	}
	else
	{
		OutputDebugStringA("Object system demo completed: rooted and cycle GC validated.\n");
	}
}

void GameApp::BeginMouseLook()
{
	if (m_is_mouse_look_active_)
	{
		return;
	}
	if (m_hwnd_ == nullptr)
	{
		return;
	}

	GetCursorPos(&m_saved_cursor_screen_position_);
	SetCapture(m_hwnd_);

	if (!m_is_cursor_hidden_)
	{
		while (ShowCursor(FALSE) >= 0)
		{
		}
		m_is_cursor_hidden_ = true;
	}

	UpdateMouseLookBounds();
	SetCursorPos(m_mouse_look_center_screen_position_.x, m_mouse_look_center_screen_position_.y);
	m_is_mouse_look_active_ = true;
}

void GameApp::EndMouseLook(bool bRestoreCursor)
{
	if (m_hwnd_ != nullptr && GetCapture() == m_hwnd_)
	{
		ReleaseCapture();
	}

	if (m_is_mouse_look_active_)
	{
		ClipCursor(nullptr);
	}

	m_is_mouse_look_active_ = false;
	if (m_is_cursor_hidden_)
	{
		while (ShowCursor(TRUE) < 0)
		{
		}
		m_is_cursor_hidden_ = false;
	}

	if (bRestoreCursor)
	{
		SetCursorPos(m_saved_cursor_screen_position_.x, m_saved_cursor_screen_position_.y);
	}
}

bool GameApp::HasActiveLevel() const
{
	return m_world_ != nullptr && m_world_->HasActiveLevel();
}

ULevel* GameApp::GetActiveLevel()
{
	if (m_world_ == nullptr)
	{
		return nullptr;
	}
	return m_world_->GetActiveLevel();
}

const ULevel* GameApp::GetActiveLevel() const
{
	if (m_world_ == nullptr)
	{
		return nullptr;
	}
	return m_world_->GetActiveLevel();
}

uint32_t GameApp::GetSceneRevision() const
{
	return m_scene_revision_;
}

uint64_t GameApp::GetSelectedActorObjectId() const
{
	return m_selected_actor_object_id_;
}

AActor* GameApp::GetSelectedActor()
{
	ULevel* ActiveLevel = GetActiveLevel();
	if (ActiveLevel == nullptr || m_selected_actor_object_id_ == 0)
	{
		return nullptr;
	}
	return ActiveLevel->FindActor(m_selected_actor_object_id_);
}

const AActor* GameApp::GetSelectedActor() const
{
	const ULevel* ActiveLevel = GetActiveLevel();
	if (ActiveLevel == nullptr || m_selected_actor_object_id_ == 0)
	{
		return nullptr;
	}
	return ActiveLevel->FindActor(m_selected_actor_object_id_);
}

void GameApp::SelectActor(uint64_t InActorObjectId)
{
	uint64_t ResolvedActorObjectId = InActorObjectId;
	if (InActorObjectId != 0)
	{
		const ULevel* ActiveLevel = GetActiveLevel();
		if (ActiveLevel == nullptr || ActiveLevel->FindActor(InActorObjectId) == nullptr)
		{
			ResolvedActorObjectId = 0;
		}
	}

	if (m_selected_actor_object_id_ == ResolvedActorObjectId)
	{
		return;
	}

	m_selected_actor_object_id_ = ResolvedActorObjectId;
}

bool GameApp::DeleteSelectedActor()
{
	const uint64_t ActorObjectId = m_selected_actor_object_id_;
	if (ActorObjectId == 0)
	{
		return false;
	}

	ULevel* ActiveLevel = GetActiveLevel();
	if (ActiveLevel == nullptr)
	{
		return false;
	}

	m_transform_gizmo_.EndDrag();
	m_is_left_mouse_down_ = false;
	m_b_left_mouse_moved_since_press_ = false;
	m_b_left_click_started_on_gizmo_ = false;

	if (!ActiveLevel->DestroyActor(ActorObjectId))
	{
		return false;
	}

	m_selected_actor_object_id_ = 0;
	m_gizmo_mesh_vertices_.clear();
	m_rotate_drag_label_ = FEditorRotateDragLabel{};
	m_renderer_.SetEditorGizmoMesh(m_gizmo_mesh_vertices_, true);
	m_renderer_.SetEditorOverlayLines({});
	m_renderer_.InvalidateSceneRenderCache();
	BumpSceneRevision();
	return true;
}

bool GameApp::SetSelectedActorTransform(const FActorTransform& InTransform, bool bBumpSceneRevision)
{
	AActor* SelectedActor = GetSelectedActor();
	if (SelectedActor == nullptr)
	{
		return false;
	}

	FActorTransform NormalizedTransform = InTransform;
	NormalizedTransform.Rotation = NormalizedTransform.Rotation.GetNormalized();
	SelectedActor->SetActorTransform(NormalizedTransform);
	if (bBumpSceneRevision)
	{
		BumpSceneRevision();
	}
	return true;
}

EGizmoMode GameApp::GetGizmoMode() const
{
	return m_gizmo_mode_;
}

void GameApp::SetGizmoMode(EGizmoMode InMode)
{
	m_gizmo_mode_ = InMode;
	m_transform_gizmo_.SetMode(InMode);
}

void GameApp::MapViewportMouseToPickScreen(int InX, int InY, float* OutPickX, float* OutPickY) const
{
	if (OutPickX == nullptr || OutPickY == nullptr)
	{
		return;
	}

	UINT ViewportWidth = 1;
	UINT ViewportHeight = 1;
	GetViewportSize(&ViewportWidth, &ViewportHeight);

	UINT HwndClientWidth = ViewportWidth;
	UINT HwndClientHeight = ViewportHeight;
	if (m_hwnd_ != nullptr)
	{
		RECT ClientRect{};
		if (GetClientRect(m_hwnd_, &ClientRect))
		{
			HwndClientWidth = static_cast<UINT>(std::max(1L, ClientRect.right - ClientRect.left));
			HwndClientHeight = static_cast<UINT>(std::max(1L, ClientRect.bottom - ClientRect.top));
		}
	}

	*OutPickX = (HwndClientWidth > 0)
		? (static_cast<float>(InX) * static_cast<float>(ViewportWidth) / static_cast<float>(HwndClientWidth))
		: static_cast<float>(InX);
	*OutPickY = (HwndClientHeight > 0)
		? (static_cast<float>(InY) * static_cast<float>(ViewportHeight) / static_cast<float>(HwndClientHeight))
		: static_cast<float>(InY);
}

void GameApp::MapPickScreenToViewportWidget(float InPickScreenX, float InPickScreenY, int* OutWidgetX, int* OutWidgetY) const
{
	if (OutWidgetX == nullptr || OutWidgetY == nullptr)
	{
		return;
	}

	UINT ViewportWidth = 1;
	UINT ViewportHeight = 1;
	GetViewportSize(&ViewportWidth, &ViewportHeight);

	UINT HwndClientWidth = ViewportWidth;
	UINT HwndClientHeight = ViewportHeight;
	if (m_hwnd_ != nullptr)
	{
		RECT ClientRect{};
		if (GetClientRect(m_hwnd_, &ClientRect))
		{
			HwndClientWidth = static_cast<UINT>(std::max(1L, ClientRect.right - ClientRect.left));
			HwndClientHeight = static_cast<UINT>(std::max(1L, ClientRect.bottom - ClientRect.top));
		}
	}

	*OutWidgetX = (ViewportWidth > 0)
		? static_cast<int>(InPickScreenX * static_cast<float>(HwndClientWidth) / static_cast<float>(ViewportWidth))
		: static_cast<int>(InPickScreenX);
	*OutWidgetY = (ViewportHeight > 0)
		? static_cast<int>(InPickScreenY * static_cast<float>(HwndClientHeight) / static_cast<float>(ViewportHeight))
		: static_cast<int>(InPickScreenY);
}

const FEditorRotateDragLabel& GameApp::GetRotateDragLabel() const
{
	return m_rotate_drag_label_;
}

void GameApp::GetViewportSize(UINT* OutWidth, UINT* OutHeight) const
{
	if (OutWidth == nullptr || OutHeight == nullptr)
	{
		return;
	}

	*OutWidth = 1;
	*OutHeight = 1;

	// Picking/gizmo unproject must match the renderer swap-chain resolution, not HWND client pixels.
	if (m_renderer_initialized_)
	{
		const UINT RenderWidth = m_renderer_.GetRenderWidth();
		const UINT RenderHeight = m_renderer_.GetRenderHeight();
		if (RenderWidth > 0 && RenderHeight > 0)
		{
			*OutWidth = RenderWidth;
			*OutHeight = RenderHeight;
			return;
		}
	}

	if (m_hwnd_ == nullptr)
	{
		return;
	}

	RECT ClientRect{};
	if (!GetClientRect(m_hwnd_, &ClientRect))
	{
		return;
	}

	*OutWidth = static_cast<UINT>(std::max(1L, ClientRect.right - ClientRect.left));
	*OutHeight = static_cast<UINT>(std::max(1L, ClientRect.bottom - ClientRect.top));
}

void GameApp::BeginCameraOrbitAroundSelection(int InMouseX, int InMouseY)
{
	const AActor* SelectedActor = GetSelectedActor();
	if (SelectedActor == nullptr)
	{
		return;
	}

	if (!ComputeCameraOrbitPivotAndRadius(m_camera_, SelectedActor, &m_camera_orbit_pivot_, &m_camera_orbit_radius_))
	{
		return;
	}

	m_b_camera_focus_active_ = false;
	m_camera_focus_elapsed_seconds_ = 0.0f;
	m_camera_velocity_.position = {0.0f, 0.0f, 0.0f};
	m_camera_orbit_last_mouse_x_ = InMouseX;
	m_camera_orbit_last_mouse_y_ = InMouseY;
	m_b_camera_orbit_active_ = true;

	if (m_hwnd_ != nullptr)
	{
		SetCapture(m_hwnd_);
	}
}

void GameApp::UpdateCameraOrbit(int InMouseX, int InMouseY)
{
	if (!m_b_camera_orbit_active_)
	{
		return;
	}

	const float MouseDx = static_cast<float>(InMouseX - m_camera_orbit_last_mouse_x_);
	const float MouseDy = static_cast<float>(InMouseY - m_camera_orbit_last_mouse_y_);
	m_camera_.yaw += MouseDx * kCameraOrbitMouseSensitivity;
	m_camera_.pitch -= MouseDy * kCameraOrbitMouseSensitivity;
	m_camera_.pitch = std::clamp(m_camera_.pitch, -1.4f, 1.4f);
	ApplyCameraOrbitPosition(&m_camera_, m_camera_orbit_pivot_, m_camera_orbit_radius_);
	m_camera_orbit_last_mouse_x_ = InMouseX;
	m_camera_orbit_last_mouse_y_ = InMouseY;
}

void GameApp::EndCameraOrbit()
{
	if (!m_b_camera_orbit_active_)
	{
		return;
	}

	if (m_hwnd_ != nullptr && GetCapture() == m_hwnd_)
	{
		ReleaseCapture();
	}

	m_b_camera_orbit_active_ = false;
	m_b_left_click_started_on_orbit_ = false;
}

void GameApp::BeginFocusOnSelectedActor()
{
	const AActor* SelectedActor = GetSelectedActor();
	if (SelectedActor == nullptr)
	{
		return;
	}

	DirectX::XMFLOAT3 TargetPosition{};
	if (!ComputeFocusTargetPosition(m_camera_, SelectedActor, &TargetPosition))
	{
		return;
	}

	m_camera_focus_start_position_ = m_camera_.position;
	m_camera_focus_target_position_ = TargetPosition;
	m_camera_focus_duration_seconds_ = ComputeFocusTransitionDuration(
		ComputeTravelDistance(m_camera_focus_start_position_, m_camera_focus_target_position_));
	m_camera_focus_elapsed_seconds_ = 0.0f;
	if (m_camera_focus_duration_seconds_ <= 0.0f)
	{
		m_camera_.position = m_camera_focus_target_position_;
		m_b_camera_focus_active_ = false;
		return;
	}

	m_b_camera_focus_active_ = true;
	m_camera_velocity_.position = {0.0f, 0.0f, 0.0f};
}

void GameApp::TickCameraFocusTransition(float InDeltaSeconds)
{
	if (!m_b_camera_focus_active_)
	{
		return;
	}

	const float Dt = std::clamp(InDeltaSeconds, 0.0f, 0.05f);
	if (Dt <= 0.0f)
	{
		return;
	}

	m_camera_focus_elapsed_seconds_ += Dt;
	const float RawAlpha = m_camera_focus_elapsed_seconds_ / m_camera_focus_duration_seconds_;
	const float Alpha = std::min(RawAlpha, 1.0f);

	m_camera_.position.x = m_camera_focus_start_position_.x +
		((m_camera_focus_target_position_.x - m_camera_focus_start_position_.x) * Alpha);
	m_camera_.position.y = m_camera_focus_start_position_.y +
		((m_camera_focus_target_position_.y - m_camera_focus_start_position_.y) * Alpha);
	m_camera_.position.z = m_camera_focus_start_position_.z +
		((m_camera_focus_target_position_.z - m_camera_focus_start_position_.z) * Alpha);

	if (RawAlpha >= 1.0f)
	{
		m_camera_.position = m_camera_focus_target_position_;
		m_b_camera_focus_active_ = false;
		m_camera_focus_elapsed_seconds_ = 0.0f;
	}
}

void GameApp::UpdateEditorKeyboard()
{
	const auto ConsumeEdge = [](int InVirtualKey, bool* InOutWasDown) -> bool
	{
		const bool bIsDown = (GetAsyncKeyState(InVirtualKey) & 0x8000) != 0;
		const bool bPressed = bIsDown && !(*InOutWasDown);
		*InOutWasDown = bIsDown;
		return bPressed;
	};

	if (m_viewport_has_focus_ && ConsumeEdge('F', &m_key_f_was_down_))
	{
		BeginFocusOnSelectedActor();
	}

	if (!m_viewport_has_focus_ || m_is_mouse_look_active_)
	{
		m_key_w_was_down_ = (GetAsyncKeyState('W') & 0x8000) != 0;
		m_key_e_was_down_ = (GetAsyncKeyState('E') & 0x8000) != 0;
		m_key_r_was_down_ = (GetAsyncKeyState('R') & 0x8000) != 0;
		if (!m_viewport_has_focus_)
		{
			m_key_f_was_down_ = (GetAsyncKeyState('F') & 0x8000) != 0;
		}
		return;
	}

	if (ConsumeEdge('W', &m_key_w_was_down_))
	{
		SetGizmoMode(EGizmoMode::Translate);
	}
	if (ConsumeEdge('E', &m_key_e_was_down_))
	{
		SetGizmoMode(EGizmoMode::Rotate);
	}
	if (ConsumeEdge('R', &m_key_r_was_down_))
	{
		SetGizmoMode(EGizmoMode::Scale);
	}
}

void GameApp::TickEditorInteraction(float DeltaSeconds)
{
	(void)DeltaSeconds;
	m_gizmo_mesh_vertices_.clear();
	m_rotate_drag_label_ = FEditorRotateDragLabel{};
	std::vector<Dx12Renderer::Vertex> OverlayLineVertices;

	AActor* SelectedActor = GetSelectedActor();
	if (SelectedActor == nullptr)
	{
		m_transform_gizmo_.EndDrag();
		m_renderer_.SetEditorGizmoMesh(m_gizmo_mesh_vertices_, true);
		m_renderer_.SetEditorOverlayLines(OverlayLineVertices);
		return;
	}

	UINT ViewportWidth = 1;
	UINT ViewportHeight = 1;
	GetViewportSize(&ViewportWidth, &ViewportHeight);
	const float NearPlane = m_renderer_.GetNearClipPlane();
	const float FarPlane = m_renderer_.GetFarClipPlane();

	float PickScreenX = 0.0f;
	float PickScreenY = 0.0f;
	MapViewportMouseToPickScreen(m_viewport_mouse_x_, m_viewport_mouse_y_, &PickScreenX, &PickScreenY);

	if (m_is_left_mouse_down_)
	{
		if (m_transform_gizmo_.IsDragging())
		{
			const FActorTransform OldTransform = SelectedActor->GetActorTransform();
			FActorTransform DragTransform;
			m_transform_gizmo_.UpdateDrag(
				SelectedActor,
				m_camera_,
				ViewportWidth,
				ViewportHeight,
				NearPlane,
				FarPlane,
				PickScreenX,
				PickScreenY,
				&DragTransform);
			const bool bTransformChanged =
				DragTransform.Position != OldTransform.Position
				|| DragTransform.Rotation != OldTransform.Rotation
				|| DragTransform.Scale != OldTransform.Scale;

			if (bTransformChanged)
			{
				FActorTransform NormalizedTransform = DragTransform;
				NormalizedTransform.Rotation = NormalizedTransform.Rotation.GetNormalized();
				SelectedActor->SetActorTransform(NormalizedTransform);
			}
		}
	}
	else if (!m_transform_gizmo_.IsDragging())
	{
		const EGizmoAxis HoveredAxis = m_transform_gizmo_.HitTest(
			SelectedActor,
			m_camera_,
			ViewportWidth,
			ViewportHeight,
			NearPlane,
			FarPlane,
			PickScreenX,
			PickScreenY);
		m_transform_gizmo_.SetHoveredAxis(HoveredAxis);
	}

	m_transform_gizmo_.BuildMeshVertices(SelectedActor, m_camera_, &m_gizmo_mesh_vertices_);
	m_transform_gizmo_.BuildRotateDragLabel(
		m_camera_,
		ViewportWidth,
		ViewportHeight,
		NearPlane,
		FarPlane,
		&m_rotate_drag_label_);

	if (m_b_show_selected_actor_aabb_debug_)
	{
		FEditorWorldAabb WorldAabb{};
		if (FEditorActorBoundsDebug::ComputeActorWorldAabb(SelectedActor, &WorldAabb))
		{
			const DirectX::XMFLOAT4 AabbColor{0.95f, 0.75f, 0.15f, 1.0f};
			FEditorActorBoundsDebug::AppendWorldAabbWireframe(WorldAabb, AabbColor, &OverlayLineVertices);
		}
	}

	if (m_b_show_selected_actor_obb_debug_)
	{
		const DirectX::XMFLOAT4 ObbColor{0.25f, 0.88f, 0.95f, 1.0f};
		FEditorActorBoundsDebug::AppendActorWorldObbWireframes(SelectedActor, ObbColor, &OverlayLineVertices);
	}

	if (m_b_show_selected_actor_section_bounds_debug_)
	{
		const DirectX::XMFLOAT4 SectionBoundsColor{0.95f, 0.35f, 0.85f, 1.0f};
		FEditorActorBoundsDebug::AppendActorWorldSectionBoundsWireframes(
			SelectedActor,
			SectionBoundsColor,
			&OverlayLineVertices);
	}

	m_renderer_.SetEditorGizmoMesh(m_gizmo_mesh_vertices_, true);
	m_renderer_.SetEditorOverlayLines(OverlayLineVertices);
}

bool GameApp::IsSelectedActorAabbDebugEnabled() const
{
	return m_b_show_selected_actor_aabb_debug_;
}

void GameApp::SetSelectedActorAabbDebugEnabled(bool bIsEnabled)
{
	m_b_show_selected_actor_aabb_debug_ = bIsEnabled;
}

bool GameApp::IsSelectedActorObbDebugEnabled() const
{
	return m_b_show_selected_actor_obb_debug_;
}

void GameApp::SetSelectedActorObbDebugEnabled(bool bIsEnabled)
{
	m_b_show_selected_actor_obb_debug_ = bIsEnabled;
}

bool GameApp::IsSelectedActorSectionBoundsDebugEnabled() const
{
	return m_b_show_selected_actor_section_bounds_debug_;
}

void GameApp::SetSelectedActorSectionBoundsDebugEnabled(bool bIsEnabled)
{
	m_b_show_selected_actor_section_bounds_debug_ = bIsEnabled;
}

uint64_t GameApp::PickActorAtViewportPosition(int InX, int InY)
{
	const ULevel* ActiveLevel = GetActiveLevel();
	if (ActiveLevel == nullptr)
	{
		return 0;
	}

	UINT ViewportWidth = 1;
	UINT ViewportHeight = 1;
	GetViewportSize(&ViewportWidth, &ViewportHeight);

	float PickScreenX = 0.0f;
	float PickScreenY = 0.0f;
	MapViewportMouseToPickScreen(InX, InY, &PickScreenX, &PickScreenY);

	const FEditorPickResult PickResult = FEditorPicking::PickActor(
		ActiveLevel,
		m_camera_,
		ViewportWidth,
		ViewportHeight,
		m_renderer_.GetNearClipPlane(),
		m_renderer_.GetFarClipPlane(),
		PickScreenX,
		PickScreenY,
		m_performance_settings_.TriangleBvhSplitMethod,
		m_scene_revision_);

	return PickResult.bHit ? PickResult.ActorObjectId : 0;
}

void GameApp::OnViewportLeftMousePress(int InX, int InY)
{
	m_is_left_mouse_down_ = true;
	m_viewport_mouse_x_ = InX;
	m_viewport_mouse_y_ = InY;
	m_left_mouse_press_x_ = InX;
	m_left_mouse_press_y_ = InY;
	m_b_left_mouse_moved_since_press_ = false;
	m_b_left_click_started_on_gizmo_ = false;
	m_b_left_click_started_on_orbit_ = false;

	AActor* SelectedActor = GetSelectedActor();
	const bool bIsAltDown =
		((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) ||
		((GetAsyncKeyState(VK_RMENU) & 0x8000) != 0);
	if (bIsAltDown && SelectedActor != nullptr)
	{
		m_b_left_click_started_on_orbit_ = true;
		BeginCameraOrbitAroundSelection(InX, InY);
		return;
	}
	UINT ViewportWidth = 1;
	UINT ViewportHeight = 1;
	GetViewportSize(&ViewportWidth, &ViewportHeight);

	float PickScreenX = 0.0f;
	float PickScreenY = 0.0f;
	MapViewportMouseToPickScreen(InX, InY, &PickScreenX, &PickScreenY);

	constexpr float kGizmoPickToleranceLoose = 1.25f;
	if (m_transform_gizmo_.IsDragging())
	{
		return;
	}

	EGizmoAxis HitAxis = EGizmoAxis::None;
	if (SelectedActor != nullptr)
	{
		HitAxis = m_transform_gizmo_.HitTest(
			SelectedActor,
			m_camera_,
			ViewportWidth,
			ViewportHeight,
			m_renderer_.GetNearClipPlane(),
			m_renderer_.GetFarClipPlane(),
			PickScreenX,
			PickScreenY,
			1.0f);
		if (HitAxis == EGizmoAxis::None)
		{
			HitAxis = m_transform_gizmo_.HitTest(
				SelectedActor,
				m_camera_,
				ViewportWidth,
				ViewportHeight,
				m_renderer_.GetNearClipPlane(),
				m_renderer_.GetFarClipPlane(),
				PickScreenX,
				PickScreenY,
				kGizmoPickToleranceLoose);
		}
		if (HitAxis != EGizmoAxis::None)
		{
			m_b_left_click_started_on_gizmo_ = true;
			if (m_transform_gizmo_.TryBeginDrag(
				SelectedActor,
				m_camera_,
				ViewportWidth,
				ViewportHeight,
				m_renderer_.GetNearClipPlane(),
				m_renderer_.GetFarClipPlane(),
				PickScreenX,
				PickScreenY,
				HitAxis,
				kGizmoPickToleranceLoose))
			{
				return;
			}

			return;
		}
	}
}

void GameApp::OnViewportLeftMouseMove(int InX, int InY)
{
	if (m_b_camera_orbit_active_)
	{
		UpdateCameraOrbit(InX, InY);
		m_b_left_mouse_moved_since_press_ = true;
		m_viewport_mouse_x_ = InX;
		m_viewport_mouse_y_ = InY;
		return;
	}

	if (m_is_left_mouse_down_)
	{
		m_b_left_mouse_moved_since_press_ = true;
	}
	m_viewport_mouse_x_ = InX;
	m_viewport_mouse_y_ = InY;
}

void GameApp::OnViewportLeftMouseRelease(int InX, int InY)
{
	m_viewport_mouse_x_ = InX;
	m_viewport_mouse_y_ = InY;

	if (m_b_camera_orbit_active_)
	{
		EndCameraOrbit();
		m_is_left_mouse_down_ = false;
		return;
	}

	const bool bShouldApplySelectionClick =
		!m_b_left_mouse_moved_since_press_ &&
		!m_b_left_click_started_on_gizmo_ &&
		!m_b_left_click_started_on_orbit_;
	if (bShouldApplySelectionClick)
	{
		AActor* SelectedActor = GetSelectedActor();
		UINT ViewportWidth = 1;
		UINT ViewportHeight = 1;
		GetViewportSize(&ViewportWidth, &ViewportHeight);

		float PickScreenX = 0.0f;
		float PickScreenY = 0.0f;
		MapViewportMouseToPickScreen(InX, InY, &PickScreenX, &PickScreenY);

		const uint64_t PickedActorId = PickActorAtViewportPosition(InX, InY);
		if (PickedActorId != 0)
		{
			SelectActor(PickedActorId);
		}
		else
		{
			constexpr float kGizmoDeselectGuardSlopPixels = 1.0f;
			EGizmoAxis ReleaseHitAxis = EGizmoAxis::None;
			if (SelectedActor != nullptr)
			{
				ReleaseHitAxis = m_transform_gizmo_.HitTest(
					SelectedActor,
					m_camera_,
					ViewportWidth,
					ViewportHeight,
					m_renderer_.GetNearClipPlane(),
					m_renderer_.GetFarClipPlane(),
					PickScreenX,
					PickScreenY,
					kGizmoDeselectGuardSlopPixels);
			}
			const bool bNearGizmo = ReleaseHitAxis != EGizmoAxis::None;
			if (!bNearGizmo)
			{
				SelectActor(0);
			}
		}
	}

	const bool bWasDraggingBeforeRelease = m_transform_gizmo_.IsDragging();

	m_is_left_mouse_down_ = false;
	m_b_left_mouse_moved_since_press_ = false;
	m_b_left_click_started_on_gizmo_ = false;
	m_transform_gizmo_.EndDrag();

	if (bWasDraggingBeforeRelease)
	{
		BumpSceneRevision();
	}
}

void GameApp::BumpSceneRevision()
{
	++m_scene_revision_;
	ValidateSelectedActor();
}

void GameApp::ValidateSelectedActor()
{
	if (m_selected_actor_object_id_ == 0)
	{
		return;
	}

	if (GetSelectedActor() == nullptr)
	{
		m_selected_actor_object_id_ = 0;
	}
}
