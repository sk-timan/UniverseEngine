#pragma once



#include <array>

#include <cstdint>

#include <filesystem>

#include <unordered_map>

#include <vector>



#include <d3d12.h>

#include <dxgi1_6.h>

#include <DirectXMath.h>

#include <wrl/client.h>



#include "components/MeshComponent.h"
#include "math/FTransform.h"
#include "world/World.h"



class UStaticMesh;
class USkeletalMesh;
class UPrimitiveComponent;



class Dx12Renderer : public IRendererInterface

{

public:
	static constexpr UINT kMaxDrawConstantSlots = 64;
	static constexpr UINT64 kDrawConstantSlotSize = 256;

	struct CameraState

	{

		DirectX::XMFLOAT3 position{};

		float yaw = 0.0f;

		float pitch = 0.0f;

	};

	struct Vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT4 color;
	};

	Dx12Renderer();

	~Dx12Renderer();



	Dx12Renderer(const Dx12Renderer&) = delete;

	Dx12Renderer& operator=(const Dx12Renderer&) = delete;



	bool Initialize(HWND InHwnd, UINT InWidth, UINT InHeight);

	void SetMapModelPath(const std::filesystem::path& InModelPath);

	bool Resize(UINT InWidth, UINT InHeight);

	void Render(const CameraState& InCamera);

	void SetEditorGizmoMesh(const std::vector<Vertex>& InVertices, bool bTriangleList = true);
	void SetEditorOverlayLines(const std::vector<Vertex>& InVertices);

	void SetCameraClipDistances(float InNearPlane, float InFarPlane);

	float GetNearClipPlane() const;

	float GetFarClipPlane() const;

	UINT GetRenderWidth() const { return m_width_; }
	UINT GetRenderHeight() const { return m_height_; }

	virtual void Render(const void* InRenderCommands, size_t InCommandCount) override;

	void InvalidateSceneRenderCache();

	bool IsDeviceLost() const;

	bool TryRecoverDevice();

private:

	struct alignas(256) SceneConstants

	{

		DirectX::XMFLOAT4X4 view_projection;

		DirectX::XMFLOAT3 light_direction;

		float ambient_strength = 0.35f;

	};



	struct alignas(256) DrawConstants

	{

		DirectX::XMFLOAT4X4 world;

		DirectX::XMFLOAT4 mesh_color;

	};



	struct MeshBuffers

	{

		Microsoft::WRL::ComPtr<ID3D12Resource> vertex_buffer;

		D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};

		UINT vertex_count = 0;

	};



	struct FIndexedMeshBuffers

	{

		Microsoft::WRL::ComPtr<ID3D12Resource> vertex_buffer;

		Microsoft::WRL::ComPtr<ID3D12Resource> index_buffer;

		D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};

		D3D12_INDEX_BUFFER_VIEW index_buffer_view{};

		UINT index_count = 0;

	};



	struct FGpuStaticMeshResources

	{

		FIndexedMeshBuffers Buffers;

	};



	struct FGpuSkeletalMeshResources

	{

		FIndexedMeshBuffers Buffers;

	};



	struct FSceneStaticDrawItem

	{

		UStaticMesh* StaticMesh = nullptr;

		UPrimitiveComponent* PrimitiveComponent = nullptr;

		FTransform WorldTransform;

		DirectX::XMFLOAT4 MeshColor{0.88f, 0.82f, 0.72f, 1.0f};

	};



	struct FSceneSkeletalDrawItem

	{

		USkeletalMesh* SkeletalMesh = nullptr;

		UPrimitiveComponent* PrimitiveComponent = nullptr;

		FTransform WorldTransform;

		DirectX::XMFLOAT4 MeshColor{0.5f, 0.72f, 0.95f, 1.0f};

	};



	struct FRetiredUploadBuffer

	{

		Microsoft::WRL::ComPtr<ID3D12Resource> Resource;

		uint64_t FenceValue = 0;

	};



	bool CreateDeviceResources(HWND InHwnd, UINT InWidth, UINT InHeight);

	bool CreateRenderTargets();

	bool CreateDepthStencil();

	bool CreateSceneResources();

	bool CreatePipelineState();

	bool CreateConstantBuffers();

	bool CreateMeshBuffers();

	bool CreateDebugAxisMesh();

	bool UpdateDebugGridMesh(const CameraState& InCamera);

	bool CreateUploadBuffer(const void* InData, UINT64 InSize, ID3D12Resource** OutUploadBuffer);

	bool CreateDefaultBufferWithInitialData(const void* InData, UINT64 InSize, D3D12_RESOURCE_STATES InFinalState, ID3D12Resource** OutResource);

	bool EnsureUploadBufferCapacity(UINT64 InRequiredSize, ID3D12Resource** InOutUploadBuffer, UINT64* InOutCapacity);

	bool UploadBufferData(ID3D12Resource* InUploadBuffer, const void* InData, UINT64 InSize);

	bool EnsureGpuStaticMeshResources(UStaticMesh* InStaticMesh);

	bool BuildStaticMeshGpuData(UStaticMesh* InStaticMesh, FGpuStaticMeshResources* OutResources);

	bool EnsureGpuSkeletalMeshResources(USkeletalMesh* InSkeletalMesh);

	bool BuildSkeletalMeshGpuData(USkeletalMesh* InSkeletalMesh, FGpuSkeletalMeshResources* OutResources);

	void ClearGpuStaticMeshCache();

	void ClearGpuSkeletalMeshCache();

	DirectX::XMFLOAT4 ResolveMeshColor(const std::vector<UMeshComponent::FMaterialOverride>& InMaterialOverrides) const;

	size_t ComputeStaticRenderCommandsHash(const void* InRenderCommands, size_t InCommandCount) const;
	size_t ComputeSkeletalRenderCommandsHash(const void* InRenderCommands, size_t InCommandCount) const;

	void UpdateCameraConstants(const CameraState& InCamera);

	void RetireUploadBuffer(Microsoft::WRL::ComPtr<ID3D12Resource>& InOutResource);

	void ReleaseCompletedUploadBuffers();

	void WaitForGpu();

	bool WaitForGpuWithTimeout(DWORD InTimeoutMs);

	void MoveToNextFrame();

	bool ExecuteUploadCopy(ID3D12Resource* InDestination, const void* InData, UINT64 InSize, D3D12_RESOURCE_STATES InFinalState);



	static constexpr UINT kFrameCount = 2;



	Microsoft::WRL::ComPtr<IDXGIFactory4> m_factory_;

	Microsoft::WRL::ComPtr<ID3D12Device> m_device_;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_command_queue_;

	Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swap_chain_;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtv_heap_;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsv_heap_;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_cbv_heap_;

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_command_list_;

	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence_;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_depth_stencil_;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_root_signature_;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_triangle_pipeline_state_;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_line_pipeline_state_;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_editor_gizmo_triangle_pipeline_state_;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_scene_constant_buffer_;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_draw_constant_buffer_;

	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kFrameCount> m_render_targets_{};

	std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, kFrameCount> m_command_allocators_{};

	std::array<uint64_t, kFrameCount> m_fence_values_{};



	MeshBuffers m_debug_line_mesh_;

	MeshBuffers m_debug_axis_mesh_;

	MeshBuffers m_editor_gizmo_mesh_;
	bool m_editor_gizmo_triangle_list_ = true;
	MeshBuffers m_editor_overlay_line_mesh_;

	int m_debug_grid_snap_cell_x_ = INT_MIN;

	int m_debug_grid_snap_cell_y_ = INT_MIN;

	float m_debug_grid_cell_size_ = 0.0f;

	int m_debug_grid_half_extent_bucket_ = INT_MIN;

	int m_debug_grid_height_bucket_ = INT_MIN;

	bool m_bDebugGridMeshReady_ = false;

	MeshBuffers m_map_model_mesh_;

	std::vector<FSceneStaticDrawItem> m_static_scene_draws_;

	std::vector<FSceneSkeletalDrawItem> m_skeletal_scene_draws_;

	std::vector<FRetiredUploadBuffer> m_retired_upload_buffers_;

	std::unordered_map<UStaticMesh*, FGpuStaticMeshResources> m_gpu_static_meshes_;

	std::unordered_map<USkeletalMesh*, FGpuSkeletalMeshResources> m_gpu_skeletal_meshes_;

	SceneConstants* m_scene_constants_cpu_ = nullptr;

	DrawConstants* m_draw_constants_cpu_ = nullptr;
	std::array<D3D12_GPU_DESCRIPTOR_HANDLE, kMaxDrawConstantSlots> m_draw_cbv_gpu_handles_{};

	std::filesystem::path m_map_model_path_{};

	uint64_t m_last_submitted_fence_value_ = 0;

	size_t m_skeletal_scene_cache_hash_ = 0;
	size_t m_static_render_cache_hash_ = 0;

	UINT m_cbv_descriptor_size_ = 0;



	UINT m_rtv_descriptor_size_ = 0;

	UINT m_dsv_descriptor_size_ = 0;

	UINT m_width_ = 0;

	UINT m_height_ = 0;

	UINT m_frame_index_ = 0;

	bool m_device_lost_ = false;

	HWND m_hwnd_ = nullptr;

	D3D12_VIEWPORT m_viewport_{};

	D3D12_RECT m_scissor_rect_{};

	HANDLE m_fence_event_ = nullptr;

	float m_near_clip_plane_ = 0.1f;

	float m_far_clip_plane_ = 250.0f;

};

