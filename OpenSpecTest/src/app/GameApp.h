#pragma once

#include <windows.h>
#include <chrono>
#include <filesystem>
#include <memory>
#include <vector>

#include "core/ObjectRegistry.h"
#include "data/EditorPreferences.h"
#include "editor/EditorPerformanceSettings.h"
#include "data/GameplayConfig.h"
#include "data/GameplayConfigStore.h"
#include "data/ResourceLoader.h"
#include "editor/EditorTransformGizmo.h"
#include "editor/EditorTypes.h"
#include "math/FTransform.h"
#include "render/Dx12Renderer.h"
#include "world/ActorTransform.h"
#include "world/World.h"

class GameApp 
{
public:
	GameApp();
	~GameApp();

	int Run(HINSTANCE InInstance, int InShowCommand);
	void Tick();
	float GetCurrentFramesPerSecond() const;
	void OnResize(UINT InWidth, UINT InHeight);
	void OnFocusGained();
	void OnFocusLost();
	void SetMouseLookActive(bool bIsActive, bool bRestoreCursor);
	float GetCameraMoveSpeed() const;
	float GetCameraSpeedScalar() const;
	void SetCameraMoveSpeed(float InSpeed);
	void SetCameraSpeedScalar(float InScalar);
	const FEditorPreferences& GetEditorPreferences() const;
	bool LoadEditorPreferences(std::string* OutErrorMessage);
	bool SaveEditorPreferences(std::string* OutErrorMessage);
	bool ApplyEditorPreferences(const FEditorPreferences& InPreferences, std::string* OutErrorMessage);
	std::filesystem::path GetEditorPreferencesIniPath() const;
	const FEditorPerformanceSettings& GetEditorPerformanceSettings() const;
	bool LoadEditorPerformanceSettings(std::string* OutErrorMessage);
	bool SaveEditorPerformanceSettings(std::string* OutErrorMessage);
	bool ApplyEditorPerformanceSettings(const FEditorPerformanceSettings& InSettings, std::string* OutErrorMessage);
	std::filesystem::path GetEditorPerformanceIniPath() const;
	const GameplayConfig& GetGameplayConfig() const;
	void SetGameplayConfig(const GameplayConfig& InConfig);
	bool LoadMapById(const std::string& InMapId, std::string* OutErrorMessage);
	bool CreateNewDefaultLevel(std::string* OutCreatedMapId, std::string* OutErrorMessage);
	bool CreateNewLevel(const std::string& InLevelId, std::string* OutErrorMessage);
	bool LoadLevelFromFile(const std::filesystem::path& InFilePath, std::string* OutErrorMessage);
	bool SaveCurrentLevel(const std::filesystem::path& InFilePath, std::string* OutErrorMessage);
	bool SaveCurrentLevelToDefault(std::string* OutErrorMessage);
	bool ImportAssetFromSourceFile(
		const std::filesystem::path& InSourceFile,
		const std::string& InContentAssetPath,
		std::string* OutSoftObjectPath,
		std::string* OutErrorMessage);
	bool LoadModelToActiveLevel(
		const std::filesystem::path& InUAssetFilePath,
		const FActorTransform& InActorTransform,
		std::string* OutErrorMessage);
	bool LoadModelToActiveLevelFromSoftPath(
		const std::string& InSoftObjectPath,
		const FActorTransform& InActorTransform,
		std::string* OutErrorMessage);
	void RefreshActiveLevelRender(bool bInvalidateSceneCache = true);
	bool ApplyGameplayConfig(const GameplayConfig& InConfig, std::string* OutErrorMessage);
	std::filesystem::path GetMapsDirectory() const;
	void Shutdown();

	bool HasActiveLevel() const;
	ULevel* GetActiveLevel();
	const ULevel* GetActiveLevel() const;
	uint32_t GetSceneRevision() const;
	uint64_t GetSelectedActorObjectId() const;
	std::vector<uint64_t> GetSelectedActorObjectIds() const;
	size_t GetSelectedActorCount() const;
	AActor* GetSelectedActor();
	const AActor* GetSelectedActor() const;
	void SelectActor(uint64_t InActorObjectId);
	void SetActorSelection(const std::vector<uint64_t>& InActorObjectIds, uint64_t InPrimaryActorObjectId);
	bool DeleteSelectedActor();
	bool SetSelectedActorTransform(const FActorTransform& InTransform, bool bBumpSceneRevision = false);
	FActorTransform GetSelectedActorEditableTransform() const;
	bool RenameActor(uint64_t InActorObjectId, const std::string& InNewName, std::string* OutErrorMessage);
	bool ReparentActor(
		uint64_t InChildActorObjectId,
		uint64_t InNewParentActorObjectId,
		std::string* OutErrorMessage);
	EGizmoMode GetGizmoMode() const;
	void SetGizmoMode(EGizmoMode InMode);
	void OnViewportLeftMousePress(int InX, int InY);
	void OnViewportLeftMouseMove(int InX, int InY);
	void OnViewportLeftMouseRelease(int InX, int InY);
	void OnViewportRightMousePress(int InX, int InY);
	void OnViewportRightMouseMove(int InX, int InY);
	void OnViewportRightMouseRelease(int InX, int InY);
	void OnViewportMouseWheel(float InAngleDeltaY);
	uint64_t PickActorAtViewportPosition(int InX, int InY);
	bool IsSelectedActorAabbDebugEnabled() const;
	void SetSelectedActorAabbDebugEnabled(bool bIsEnabled);
	bool IsSelectedActorObbDebugEnabled() const;
	void SetSelectedActorObbDebugEnabled(bool bIsEnabled);
	bool IsSelectedActorSectionBoundsDebugEnabled() const;
	void SetSelectedActorSectionBoundsDebugEnabled(bool bIsEnabled);
	const FEditorRotateDragLabel& GetRotateDragLabel() const;
	void MapPickScreenToViewportWidget(float InPickScreenX, float InPickScreenY, int* OutWidgetX, int* OutWidgetY) const;
	bool ComputeViewportDropSpawnPosition(int InPhysicalX, int InPhysicalY, FVector3* OutWorldPosition) const;

private:
	void GetViewportSize(UINT* OutWidth, UINT* OutHeight) const;
	void MapViewportMouseToPickScreen(int InX, int InY, float* OutPickX, float* OutPickY) const;
	void UpdateEditorKeyboard();
	void BeginFocusOnSelectedActor();
	void TickCameraFocusTransition(float InDeltaSeconds);
	void BeginCameraOrbitAroundSelection(int InMouseX, int InMouseY);
	void UpdateCameraOrbit(int InMouseX, int InMouseY);
	void EndCameraOrbit();
	void BeginCameraDollyAlongTargetAxis(int InMouseX, int InMouseY);
	void UpdateCameraDollyAlongTargetAxis(int InMouseX, int InMouseY);
	void EndCameraDollyAlongTargetAxis();
	void UpdateInput(float DeltaSeconds);
	void TickEditorInteraction(float DeltaSeconds);
	void BumpSceneRevision();
	void ValidateSelectedActor();
	void BeginMultiSelectDragSnapshot(const AActor* InPrimaryActor);
	void ApplyMultiSelectFollowersFromPrimary(const AActor* InPrimaryActor);
	void ClearMultiSelectDragSnapshot();
	bool InitializeDataDrivenResources(std::wstring* OutErrorMessage);
	bool EnsureLevelDefinitionForMapId(const std::string& InMapId, std::string* OutErrorMessage);
	bool ResolveMapModelPathByMapId(const std::string& InMapId, std::filesystem::path* OutMapModelPath, std::string* OutErrorMessage) const;
	void SyncRendererLevelInput();
	void BeginMouseLook();
	void EndMouseLook(bool bRestoreCursor);
	void UpdateMouseLookBounds();
	void RunObjectSystemDemo();

	HWND m_hwnd_ = nullptr;
	bool m_renderer_initialized_ = false;
	bool m_has_device_lost_reported_ = false;
	bool m_is_minimized_ = false;
	bool m_has_pending_resize_ = false;
	bool m_viewport_has_focus_ = false;
	bool m_is_mouse_look_active_ = false;
	bool m_is_cursor_hidden_ = false;
	UINT m_pending_resize_width_ = 0;
	UINT m_pending_resize_height_ = 0;
	std::chrono::steady_clock::time_point m_next_resize_attempt_time_{};
	std::chrono::steady_clock::time_point m_next_device_recovery_attempt_time_{};
	POINT m_saved_cursor_screen_position_{};
	POINT m_mouse_look_center_screen_position_{};
	Dx12Renderer::CameraState m_camera_{};
	Dx12Renderer::CameraState m_camera_velocity_{};
	float m_camera_move_speed_ = 8.0f;
	float m_camera_speed_scalar_ = 1.0f;
	FEditorPreferences m_editor_preferences_{};
	std::filesystem::path m_editor_preferences_ini_path_{};
	FEditorPerformanceSettings m_performance_settings_{};
	std::filesystem::path m_performance_settings_ini_path_{};
	std::chrono::steady_clock::time_point m_last_frame_time_{};
	float m_current_frames_per_second_ = 0.0f;
	float m_fps_sample_accumulator_seconds_ = 0.0f;
	uint32_t m_fps_sample_frame_count_ = 0;
	ResourceLoader m_resource_loader_{};
	GameplayConfig m_gameplay_config_{};
	std::filesystem::path m_gameplay_config_path_{};
	std::filesystem::path m_executable_dir_{};
	std::filesystem::path m_default_map_model_path_{};
	std::filesystem::path m_maps_directory_{};
	std::filesystem::path m_current_level_save_path_{};
	uint32_t m_runtime_new_level_serial_ = 1;
	std::vector<ResourceLoader::FishSpeciesDef> m_fish_species_defs_{};
	ResourceLoader::ModelAsset m_rod_model_asset_{};
	bool m_has_run_object_system_demo_ = false;
	ObjectRegistry m_object_registry_{};
	std::unique_ptr<UWorld> m_world_;
	std::unique_ptr<class MainWindow> m_main_window_;
	Dx12Renderer m_renderer_;
	std::vector<uint64_t> m_selected_actor_object_ids_;
	uint64_t m_primary_selected_actor_object_id_ = 0;
	struct FMultiSelectDragFollower
	{
		uint64_t ActorObjectId = 0;
		FTransform RelativeToPrimaryWorld{};
	};
	std::vector<FMultiSelectDragFollower> m_multi_select_drag_followers_;
	uint32_t m_scene_revision_ = 0;
	EGizmoMode m_gizmo_mode_ = EGizmoMode::Translate;
	FEditorTransformGizmo m_transform_gizmo_{};
	bool m_is_left_mouse_down_ = false;
	int m_viewport_mouse_x_ = 0;
	int m_viewport_mouse_y_ = 0;
	int m_left_mouse_press_x_ = 0;
	int m_left_mouse_press_y_ = 0;
	bool m_b_left_mouse_moved_since_press_ = false;
	bool m_b_left_click_started_on_gizmo_ = false;
	bool m_b_left_click_started_on_orbit_ = false;
	bool m_b_camera_orbit_active_ = false;
	FVector3 m_camera_orbit_pivot_{};
	float m_camera_orbit_radius_ = 0.0f;
	int m_camera_orbit_last_mouse_x_ = 0;
	int m_camera_orbit_last_mouse_y_ = 0;
	bool m_b_camera_dolly_active_ = false;
	FVector3 m_camera_dolly_pivot_{};
	float m_camera_dolly_radius_ = 0.0f;
	int m_camera_dolly_last_mouse_x_ = 0;
	int m_camera_dolly_last_mouse_y_ = 0;
	bool m_key_w_was_down_ = false;
	bool m_key_e_was_down_ = false;
	bool m_key_r_was_down_ = false;
	bool m_key_f_was_down_ = false;
	bool m_b_camera_focus_active_ = false;
	float m_camera_focus_elapsed_seconds_ = 0.0f;
	float m_camera_focus_duration_seconds_ = 0.0f;
	DirectX::XMFLOAT3 m_camera_focus_start_position_{};
	DirectX::XMFLOAT3 m_camera_focus_target_position_{};
	std::vector<Dx12Renderer::Vertex> m_gizmo_mesh_vertices_{};
	bool m_b_show_selected_actor_aabb_debug_ = false;
	bool m_b_show_selected_actor_obb_debug_ = false;
	bool m_b_show_selected_actor_section_bounds_debug_ = false;
	FEditorRotateDragLabel m_rotate_drag_label_{};
};
