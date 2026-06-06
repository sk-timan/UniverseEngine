#include "render/Dx12Renderer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

#include <d3dcompiler.h>

#include "components/MeshComponent.h"
#include "components/PrimitiveComponent.h"
#include "data/AssimpCoordinate.h"
#include "render/asset/StaticMesh.h"
#include "render/asset/SkeletalMesh.h"
#include "editor/EditorViewMatrices.h"
#include "render/RenderCollector.h"

namespace {
using namespace DirectX;

Dx12Renderer::Vertex ConvertSkeletalMeshVertexLocal(
	const USkeletalMesh::FSkinVertex& InVertex,
	const DirectX::XMFLOAT4& InMeshColor)
{
	Dx12Renderer::Vertex Vertex{};
	Vertex.position = {InVertex.Position.X, InVertex.Position.Y, InVertex.Position.Z};
	Vertex.normal = {InVertex.Normal.X, InVertex.Normal.Y, InVertex.Normal.Z};
	Vertex.color = InMeshColor;
	return Vertex;
}

struct FShaderCompileArgs
{
	const std::filesystem::path* InShaderPath = nullptr;
	const char* InEntryPoint = nullptr;
	const char* InShaderTarget = nullptr;
	UINT InCompileFlags = 0;
	ID3DBlob** OutCompiledShader = nullptr;
};

bool IsSoftwareAdapter(IDXGIAdapter1* InAdapter)
{
	DXGI_ADAPTER_DESC1 Desc{};
	InAdapter->GetDesc1(&Desc);
	return (Desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
}

bool CompileShaderFromFile(const FShaderCompileArgs& InArgs)
{
	Microsoft::WRL::ComPtr<ID3DBlob> ErrorBlob;
	return SUCCEEDED(
		D3DCompileFromFile(
			InArgs.InShaderPath->c_str(),
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			InArgs.InEntryPoint,
			InArgs.InShaderTarget,
			InArgs.InCompileFlags,
			0,
			InArgs.OutCompiledShader,
			&ErrorBlob));
}

void UpdateViewportState(UINT InWidth, UINT InHeight, D3D12_VIEWPORT* OutViewport, D3D12_RECT* OutScissor)
{
	OutViewport->TopLeftX = 0.0f;
	OutViewport->TopLeftY = 0.0f;
	OutViewport->Width = static_cast<float>(InWidth);
	OutViewport->Height = static_cast<float>(InHeight);
	OutViewport->MinDepth = 0.0f;
	OutViewport->MaxDepth = 1.0f;

	OutScissor->left = 0;
	OutScissor->top = 0;
	OutScissor->right = static_cast<LONG>(InWidth);
	OutScissor->bottom = static_cast<LONG>(InHeight);
}

UINT64 AlignTo(UINT64 InValue, UINT64 InAlignment)
{
	return (InValue + InAlignment - 1) & ~(InAlignment - 1);
}

bool IsDeviceRemovedHr(HRESULT InHr)
{
	return InHr == DXGI_ERROR_DEVICE_REMOVED || InHr == DXGI_ERROR_DEVICE_RESET;
}

size_t HashCombine(size_t InSeed, size_t InValue)
{
	return InSeed ^ (InValue + 0x9e3779b9 + (InSeed << 6) + (InSeed >> 2));
}

size_t HashFloat(size_t InSeed, float InValue)
{
	const uint32_t Bits = *reinterpret_cast<const uint32_t*>(&InValue);
	return HashCombine(InSeed, static_cast<size_t>(Bits));
}

size_t HashTransform(size_t InSeed, const FTransform& InTransform)
{
	size_t Hash = InSeed;
	const FVector3 Location = InTransform.GetLocation();
	const FRotator3 Rotation = InTransform.GetRotation();
	const FVector3 Scale = InTransform.GetScale();
	Hash = HashFloat(Hash, Location.X);
	Hash = HashFloat(Hash, Location.Y);
	Hash = HashFloat(Hash, Location.Z);
	Hash = HashFloat(Hash, Rotation.Pitch);
	Hash = HashFloat(Hash, Rotation.Yaw);
	Hash = HashFloat(Hash, Rotation.Roll);
	Hash = HashFloat(Hash, Scale.X);
	Hash = HashFloat(Hash, Scale.Y);
	Hash = HashFloat(Hash, Scale.Z);
	return Hash;
}

} // namespace

Dx12Renderer::Dx12Renderer() = default;

Dx12Renderer::~Dx12Renderer()
{
	(void)WaitForGpuWithTimeout(1000);
	ReleaseCompletedUploadBuffers();
	ClearGpuStaticMeshCache();
	if (m_scene_constant_buffer_ != nullptr && m_scene_constants_cpu_ != nullptr)
	{
		m_scene_constant_buffer_->Unmap(0, nullptr);
		m_scene_constants_cpu_ = nullptr;
	}
	if (m_draw_constant_buffer_ != nullptr && m_draw_constants_cpu_ != nullptr)
	{
		m_draw_constant_buffer_->Unmap(0, nullptr);
		m_draw_constants_cpu_ = nullptr;
	}
	m_draw_cbv_gpu_handles_ = {};
	if (m_fence_event_ != nullptr)
	{
		CloseHandle(m_fence_event_);
		m_fence_event_ = nullptr;
	}
}

bool Dx12Renderer::Initialize(HWND InHwnd, UINT InWidth, UINT InHeight)
{
	m_hwnd_ = InHwnd;
	return CreateDeviceResources(InHwnd, InWidth, InHeight);
}

void Dx12Renderer::SetMapModelPath(const std::filesystem::path& InModelPath)
{
	(void)InModelPath;
}

DirectX::XMFLOAT4 Dx12Renderer::ResolveMeshColor(const std::vector<UMeshComponent::FMaterialOverride>& InMaterialOverrides) const
{
	if (!InMaterialOverrides.empty())
	{
		const float TintSeed = static_cast<float>((InMaterialOverrides[0].MaterialSlot % 5) + 1) / 5.0f;
		return DirectX::XMFLOAT4(0.45f + (0.25f * TintSeed), 0.55f + (0.15f * TintSeed), 0.72f, 1.0f);
	}

	return DirectX::XMFLOAT4(0.88f, 0.82f, 0.72f, 1.0f);
}

size_t Dx12Renderer::ComputeStaticRenderCommandsHash(const void* InRenderCommands, size_t InCommandCount) const
{
	if (InRenderCommands == nullptr || InCommandCount == 0)
	{
		return 0;
	}

	const FMeshDrawCommand* Commands = static_cast<const FMeshDrawCommand*>(InRenderCommands);
	size_t Hash = 0;
	for (size_t CommandIndex = 0; CommandIndex < InCommandCount; ++CommandIndex)
	{
		const FMeshDrawCommand& Command = Commands[CommandIndex];
		if (Command.StaticMesh == nullptr)
		{
			continue;
		}

		Hash = HashCombine(Hash, reinterpret_cast<size_t>(Command.StaticMesh));
		Hash = HashTransform(Hash, Command.WorldTransform);
		Hash = HashCombine(Hash, Command.MaterialOverrides.size());
	}

	return Hash;
}

size_t Dx12Renderer::ComputeSkeletalRenderCommandsHash(const void* InRenderCommands, size_t InCommandCount) const
{
	if (InRenderCommands == nullptr || InCommandCount == 0)
	{
		return 0;
	}

	const FMeshDrawCommand* Commands = static_cast<const FMeshDrawCommand*>(InRenderCommands);
	size_t Hash = 0;
	for (size_t CommandIndex = 0; CommandIndex < InCommandCount; ++CommandIndex)
	{
		const FMeshDrawCommand& Command = Commands[CommandIndex];
		if (Command.SkeletalMesh == nullptr)
		{
			continue;
		}

		Hash = HashCombine(Hash, reinterpret_cast<size_t>(Command.SkeletalMesh));
		Hash = HashTransform(Hash, Command.WorldTransform);
		Hash = HashCombine(Hash, Command.MaterialOverrides.size());
		Hash = HashCombine(Hash, Command.SkeletalMesh->GetSkinVertices().size());
		Hash = HashCombine(Hash, Command.SkeletalMesh->GetIndices().size());
	}

	return Hash;
}

void Dx12Renderer::Render(const void* InRenderCommands, size_t InCommandCount)
{
	ReleaseCompletedUploadBuffers();

	if (InRenderCommands == nullptr || InCommandCount == 0)
	{
		m_static_scene_draws_.clear();
		m_skeletal_scene_draws_.clear();
		m_skeletal_scene_cache_hash_ = 0;
		m_static_render_cache_hash_ = 0;
		return;
	}

	const FMeshDrawCommand* Commands = static_cast<const FMeshDrawCommand*>(InRenderCommands);
	const size_t StaticHash = ComputeStaticRenderCommandsHash(InRenderCommands, InCommandCount);
	const size_t SkeletalHash = ComputeSkeletalRenderCommandsHash(InRenderCommands, InCommandCount);
	const bool bStaticSceneUnchanged =
		StaticHash == m_static_render_cache_hash_ && !m_static_scene_draws_.empty();
	const bool bSkeletalSceneUnchanged =
		SkeletalHash == m_skeletal_scene_cache_hash_ && !m_skeletal_scene_draws_.empty();

	if (!bStaticSceneUnchanged)
	{
		m_static_scene_draws_.clear();
		for (size_t CommandIndex = 0; CommandIndex < InCommandCount; ++CommandIndex)
		{
			const FMeshDrawCommand& Command = Commands[CommandIndex];
			if (Command.StaticMesh == nullptr || !Command.StaticMesh->HasResidentGeometryData())
			{
				continue;
			}
			if (!EnsureGpuStaticMeshResources(Command.StaticMesh))
			{
				continue;
			}

			FSceneStaticDrawItem DrawItem;
			DrawItem.StaticMesh = Command.StaticMesh;
			DrawItem.PrimitiveComponent = Command.PrimitiveComponent;
			DrawItem.WorldTransform = Command.WorldTransform;
			if (Command.PrimitiveComponent != nullptr)
			{
				DrawItem.WorldTransform = Command.PrimitiveComponent->GetWorldTransform();
			}
			DrawItem.MeshColor = ResolveMeshColor(Command.MaterialOverrides);
			m_static_scene_draws_.push_back(DrawItem);
		}
		m_static_render_cache_hash_ = StaticHash;
	}

	if (!bSkeletalSceneUnchanged)
	{
		m_skeletal_scene_draws_.clear();
		for (size_t CommandIndex = 0; CommandIndex < InCommandCount; ++CommandIndex)
		{
			const FMeshDrawCommand& Command = Commands[CommandIndex];
			if (Command.SkeletalMesh == nullptr || !Command.SkeletalMesh->HasResidentGeometryData())
			{
				continue;
			}
			if (!EnsureGpuSkeletalMeshResources(Command.SkeletalMesh))
			{
				continue;
			}

			FSceneSkeletalDrawItem DrawItem;
			DrawItem.SkeletalMesh = Command.SkeletalMesh;
			DrawItem.PrimitiveComponent = Command.PrimitiveComponent;
			DrawItem.WorldTransform = Command.WorldTransform;
			if (Command.PrimitiveComponent != nullptr)
			{
				DrawItem.WorldTransform = Command.PrimitiveComponent->GetWorldTransform();
			}
			DrawItem.MeshColor = ResolveMeshColor(Command.MaterialOverrides);
			m_skeletal_scene_draws_.push_back(DrawItem);
		}
		m_skeletal_scene_cache_hash_ = SkeletalHash;
	}
}

void Dx12Renderer::InvalidateSceneRenderCache()
{
	m_skeletal_scene_cache_hash_ = 0;
	m_static_render_cache_hash_ = 0;
	m_static_scene_draws_.clear();
	m_skeletal_scene_draws_.clear();
}

bool Dx12Renderer::IsDeviceLost() const
{
	return m_device_lost_;
}

bool Dx12Renderer::TryRecoverDevice()
{
	if (!m_device_lost_)
	{
		return true;
	}
	if (m_hwnd_ == nullptr)
	{
		return false;
	}

	if (m_scene_constant_buffer_ != nullptr && m_scene_constants_cpu_ != nullptr)
	{
		m_scene_constant_buffer_->Unmap(0, nullptr);
		m_scene_constants_cpu_ = nullptr;
	}
	if (m_draw_constant_buffer_ != nullptr && m_draw_constants_cpu_ != nullptr)
	{
		m_draw_constant_buffer_->Unmap(0, nullptr);
		m_draw_constants_cpu_ = nullptr;
	}
	if (m_fence_event_ != nullptr)
	{
		CloseHandle(m_fence_event_);
		m_fence_event_ = nullptr;
	}

	m_command_list_.Reset();
	m_cbv_heap_.Reset();
	m_scene_constant_buffer_.Reset();
	m_draw_constant_buffer_.Reset();
	m_draw_constants_cpu_ = nullptr;
	m_draw_cbv_gpu_handles_ = {};
	m_root_signature_.Reset();
	m_triangle_pipeline_state_.Reset();
	m_line_pipeline_state_.Reset();
	m_editor_gizmo_triangle_pipeline_state_.Reset();
	m_depth_stencil_.Reset();
	m_fence_.Reset();
	m_swap_chain_.Reset();
	m_rtv_heap_.Reset();
	m_dsv_heap_.Reset();
	m_command_queue_.Reset();
	m_device_.Reset();
	m_factory_.Reset();
	for (auto& RenderTarget : m_render_targets_)
	{
		RenderTarget.Reset();
	}
	for (auto& CommandAllocator : m_command_allocators_)
	{
		CommandAllocator.Reset();
	}
	m_fence_values_.fill(0);
	m_debug_line_mesh_ = {};
	m_debug_axis_mesh_ = {};
	m_debug_grid_snap_cell_x_ = INT_MIN;
	m_debug_grid_snap_cell_y_ = INT_MIN;
	m_debug_grid_cell_size_ = 0.0f;
	m_debug_grid_half_extent_bucket_ = INT_MIN;
	m_debug_grid_height_bucket_ = INT_MIN;
	m_bDebugGridMeshReady_ = false;
	m_map_model_mesh_ = {};
	m_retired_upload_buffers_.clear();
	m_last_submitted_fence_value_ = 0;
	m_skeletal_scene_cache_hash_ = 0;
	ClearGpuStaticMeshCache();
	ClearGpuSkeletalMeshCache();

	const UINT RecoverWidth = (m_width_ > 0) ? m_width_ : 1;
	const UINT RecoverHeight = (m_height_ > 0) ? m_height_ : 1;
	if (!CreateDeviceResources(m_hwnd_, RecoverWidth, RecoverHeight))
	{
		m_device_lost_ = true;
		return false;
	}

	m_device_lost_ = false;
	return true;
}

bool Dx12Renderer::Resize(UINT InWidth, UINT InHeight)
{
	if (m_device_lost_)
	{
		return false;
	}
	if (m_swap_chain_ == nullptr || InWidth == 0 || InHeight == 0)
	{
		return false;
	}
	if (InWidth == m_width_ && InHeight == m_height_)
	{
		return true;
	}

	if (!WaitForGpuWithTimeout(16))
	{
		return false;
	}

	for (auto& RenderTarget : m_render_targets_)
	{
		RenderTarget.Reset();
	}
	m_depth_stencil_.Reset();

	const HRESULT ResizeHr = m_swap_chain_->ResizeBuffers(
		kFrameCount, InWidth, InHeight, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	if (FAILED(ResizeHr))
	{
		if (IsDeviceRemovedHr(ResizeHr))
		{
			m_device_lost_ = true;
		}
		return false;
	}

	m_frame_index_ = m_swap_chain_->GetCurrentBackBufferIndex();
	m_width_ = InWidth;
	m_height_ = InHeight;
	UpdateViewportState(m_width_, m_height_, &m_viewport_, &m_scissor_rect_);

	if (!CreateRenderTargets())
	{
		return false;
	}
	(void)CreateDepthStencil();
	return true;
}

bool Dx12Renderer::CreateDeviceResources(HWND InHwnd, UINT InWidth, UINT InHeight)
{
	UINT FactoryFlags = 0;
#if defined(_DEBUG)
	Microsoft::WRL::ComPtr<ID3D12Debug> DebugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&DebugController))))
	{
		DebugController->EnableDebugLayer();
		FactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
#endif

	if (FAILED(CreateDXGIFactory2(FactoryFlags, IID_PPV_ARGS(&m_factory_))))
	{
		return false;
	}

	Microsoft::WRL::ComPtr<IDXGIAdapter1> Adapter;
	for (UINT AdapterIndex = 0;
		 m_factory_->EnumAdapters1(AdapterIndex, &Adapter) != DXGI_ERROR_NOT_FOUND;
		 ++AdapterIndex)
	{
		if (IsSoftwareAdapter(Adapter.Get()))
		{
			Adapter.Reset();
			continue;
		}

		if (SUCCEEDED(D3D12CreateDevice(
				Adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device_))))
		{
			break;
		}
		Adapter.Reset();
	}

	if (m_device_ == nullptr)
	{
		return false;
	}

	D3D12_COMMAND_QUEUE_DESC QueueDesc{};
	QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	if (FAILED(m_device_->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&m_command_queue_))))
	{
		return false;
	}

	DXGI_SWAP_CHAIN_DESC1 SwapChainDesc{};
	SwapChainDesc.BufferCount = kFrameCount;
	SwapChainDesc.Width = InWidth;
	SwapChainDesc.Height = InHeight;
	SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	SwapChainDesc.SampleDesc.Count = 1;

	Microsoft::WRL::ComPtr<IDXGISwapChain1> SwapChain1;
	if (FAILED(m_factory_->CreateSwapChainForHwnd(
			m_command_queue_.Get(), InHwnd, &SwapChainDesc, nullptr, nullptr, &SwapChain1)))
	{
		return false;
	}
	if (FAILED(m_factory_->MakeWindowAssociation(InHwnd, DXGI_MWA_NO_ALT_ENTER)))
	{
		return false;
	}
	if (FAILED(SwapChain1.As(&m_swap_chain_)))
	{
		return false;
	}
	m_frame_index_ = m_swap_chain_->GetCurrentBackBufferIndex();
	m_width_ = InWidth;
	m_height_ = InHeight;
	UpdateViewportState(m_width_, m_height_, &m_viewport_, &m_scissor_rect_);

	D3D12_DESCRIPTOR_HEAP_DESC RtvHeapDesc{};
	RtvHeapDesc.NumDescriptors = kFrameCount;
	RtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	if (FAILED(m_device_->CreateDescriptorHeap(&RtvHeapDesc, IID_PPV_ARGS(&m_rtv_heap_))))
	{
		return false;
	}
	m_rtv_descriptor_size_ =
		m_device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_DESCRIPTOR_HEAP_DESC DsvHeapDesc{};
	DsvHeapDesc.NumDescriptors = 1;
	DsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	if (FAILED(m_device_->CreateDescriptorHeap(&DsvHeapDesc, IID_PPV_ARGS(&m_dsv_heap_))))
	{
		return false;
	}
	m_dsv_descriptor_size_ =
		m_device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	for (UINT Frame = 0; Frame < kFrameCount; ++Frame)
	{
		if (FAILED(m_device_->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_command_allocators_[Frame]))))
		{
			return false;
		}
	}

	if (!CreateRenderTargets())
	{
		return false;
	}
	if (!CreateDepthStencil())
	{
		return false;
	}

	if (FAILED(m_device_->CreateCommandList(0,
											D3D12_COMMAND_LIST_TYPE_DIRECT,
											m_command_allocators_[m_frame_index_].Get(),
											nullptr,
											IID_PPV_ARGS(&m_command_list_))))
	{
		return false;
	}
	if (FAILED(m_command_list_->Close()))
	{
		return false;
	}

	if (!CreateSceneResources())
	{
		return false;
	}

	if (FAILED(m_device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence_))))
	{
		return false;
	}
	m_fence_values_.fill(0);
	m_fence_values_[m_frame_index_] = 1;

	m_fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	return m_fence_event_ != nullptr;
}

bool Dx12Renderer::CreateSceneResources()
{
	return CreatePipelineState() && CreateConstantBuffers() && CreateMeshBuffers();
}

bool Dx12Renderer::CreatePipelineState()
{
	UINT compile_flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
	compile_flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	const std::filesystem::path shader_root = std::filesystem::current_path() / "Shader";
	const std::filesystem::path vertex_shader_path = shader_root / "ColorPassVS.hlsl";
	const std::filesystem::path pixel_shader_path = shader_root / "ColorPassPS.hlsl";

	Microsoft::WRL::ComPtr<ID3DBlob> vertex_shader;
	Microsoft::WRL::ComPtr<ID3DBlob> pixel_shader;
	if (!CompileShaderFromFile({&vertex_shader_path, "main", "vs_5_0", compile_flags, vertex_shader.ReleaseAndGetAddressOf()}))
	{
		return false;
	}

	if (!CompileShaderFromFile({&pixel_shader_path, "main", "ps_5_0", compile_flags, pixel_shader.ReleaseAndGetAddressOf()}))
	{
		return false;
	}

	D3D12_DESCRIPTOR_RANGE SceneCbvRange{};
	SceneCbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	SceneCbvRange.NumDescriptors = 1;
	SceneCbvRange.BaseShaderRegister = 0;
	SceneCbvRange.RegisterSpace = 0;
	SceneCbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE DrawCbvRange{};
	DrawCbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	DrawCbvRange.NumDescriptors = 1;
	DrawCbvRange.BaseShaderRegister = 1;
	DrawCbvRange.RegisterSpace = 0;
	DrawCbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER RootParameters[2]{};
	RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[0].DescriptorTable.pDescriptorRanges = &SceneCbvRange;
	RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	RootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	RootParameters[1].DescriptorTable.pDescriptorRanges = &DrawCbvRange;
	RootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC root_signature_desc{};
	root_signature_desc.NumParameters = 2;
	root_signature_desc.pParameters = RootParameters;
	root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	Microsoft::WRL::ComPtr<ID3DBlob> signature_blob;
	Microsoft::WRL::ComPtr<ID3DBlob> error_blob;
	if (FAILED(D3D12SerializeRootSignature(
			&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature_blob, &error_blob)))
	{
		return false;
	}
	if (FAILED(m_device_->CreateRootSignature(
			0, signature_blob->GetBufferPointer(), signature_blob->GetBufferSize(), IID_PPV_ARGS(&m_root_signature_))))
	{
		return false;
	}

	const D3D12_INPUT_ELEMENT_DESC input_layout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
	pso_desc.pRootSignature = m_root_signature_.Get();
	pso_desc.VS = {vertex_shader->GetBufferPointer(), vertex_shader->GetBufferSize()};
	pso_desc.PS = {pixel_shader->GetBufferPointer(), pixel_shader->GetBufferSize()};
	pso_desc.BlendState.AlphaToCoverageEnable = FALSE;
	pso_desc.BlendState.IndependentBlendEnable = FALSE;
	const D3D12_RENDER_TARGET_BLEND_DESC default_blend_desc = {
		FALSE,
		FALSE,
		D3D12_BLEND_ONE,
		D3D12_BLEND_ZERO,
		D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE,
		D3D12_BLEND_ZERO,
		D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP,
		D3D12_COLOR_WRITE_ENABLE_ALL};
	for (auto& blend_desc : pso_desc.BlendState.RenderTarget)
	{
		blend_desc = default_blend_desc;
	}
	pso_desc.SampleMask = UINT_MAX;
	pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	pso_desc.RasterizerState.FrontCounterClockwise = FALSE;
	pso_desc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	pso_desc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	pso_desc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	pso_desc.RasterizerState.DepthClipEnable = TRUE;
	pso_desc.RasterizerState.MultisampleEnable = FALSE;
	pso_desc.RasterizerState.AntialiasedLineEnable = FALSE;
	pso_desc.RasterizerState.ForcedSampleCount = 0;
	pso_desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	pso_desc.DepthStencilState.DepthEnable = TRUE;
	pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	pso_desc.DepthStencilState.StencilEnable = FALSE;
	pso_desc.InputLayout = {input_layout, _countof(input_layout)};
	pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso_desc.NumRenderTargets = 1;
	pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pso_desc.SampleDesc.Count = 1;

	if (FAILED(m_device_->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&m_triangle_pipeline_state_))))
	{
		return false;
	}

	pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	for (auto& BlendDesc : pso_desc.BlendState.RenderTarget)
	{
		BlendDesc.BlendEnable = TRUE;
		BlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		BlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		BlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		BlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
		BlendDesc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		BlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		BlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}
	if (FAILED(m_device_->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&m_line_pipeline_state_))))
	{
		return false;
	}

	pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	if (FAILED(m_device_->CreateGraphicsPipelineState(
			&pso_desc,
			IID_PPV_ARGS(&m_editor_gizmo_triangle_pipeline_state_))))
	{
		return false;
	}

	return true;
}

bool Dx12Renderer::CreateConstantBuffers()
{
	D3D12_DESCRIPTOR_HEAP_DESC cbv_heap_desc{};
	cbv_heap_desc.NumDescriptors = 1 + kMaxDrawConstantSlots;
	cbv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	if (FAILED(m_device_->CreateDescriptorHeap(&cbv_heap_desc, IID_PPV_ARGS(&m_cbv_heap_))))
	{
		return false;
	}

	m_cbv_descriptor_size_ = m_device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	const UINT64 SceneBufferSize = AlignTo(sizeof(SceneConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	const UINT64 DrawBufferSize = kMaxDrawConstantSlots * kDrawConstantSlotSize;

	D3D12_HEAP_PROPERTIES HeapProps{};
	HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	auto CreateMappedConstantBuffer = [this, &HeapProps](UINT64 InSize, ID3D12Resource** OutBuffer, void** OutMapped) -> bool
	{
		D3D12_RESOURCE_DESC BufferDesc{};
		BufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		BufferDesc.Width = InSize;
		BufferDesc.Height = 1;
		BufferDesc.DepthOrArraySize = 1;
		BufferDesc.MipLevels = 1;
		BufferDesc.SampleDesc.Count = 1;
		BufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		if (FAILED(m_device_->CreateCommittedResource(&HeapProps,
													  D3D12_HEAP_FLAG_NONE,
													  &BufferDesc,
													  D3D12_RESOURCE_STATE_GENERIC_READ,
													  nullptr,
													  IID_PPV_ARGS(OutBuffer))))
		{
			return false;
		}

		return SUCCEEDED((*OutBuffer)->Map(0, nullptr, OutMapped));
	};

	if (!CreateMappedConstantBuffer(SceneBufferSize, m_scene_constant_buffer_.ReleaseAndGetAddressOf(), reinterpret_cast<void**>(&m_scene_constants_cpu_)))
	{
		return false;
	}

	if (!CreateMappedConstantBuffer(DrawBufferSize, m_draw_constant_buffer_.ReleaseAndGetAddressOf(), reinterpret_cast<void**>(&m_draw_constants_cpu_)))
	{
		return false;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC SceneCbvDesc{};
	SceneCbvDesc.BufferLocation = m_scene_constant_buffer_->GetGPUVirtualAddress();
	SceneCbvDesc.SizeInBytes = static_cast<UINT>(SceneBufferSize);
	m_device_->CreateConstantBufferView(&SceneCbvDesc, m_cbv_heap_->GetCPUDescriptorHandleForHeapStart());

	D3D12_GPU_DESCRIPTOR_HANDLE DrawCbvHeapStartGpu = m_cbv_heap_->GetGPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE DrawCbvHeapStartCpu = m_cbv_heap_->GetCPUDescriptorHandleForHeapStart();
	DrawCbvHeapStartCpu.ptr += static_cast<SIZE_T>(m_cbv_descriptor_size_);
	DrawCbvHeapStartGpu.ptr += static_cast<UINT64>(m_cbv_descriptor_size_);

	const UINT DrawCbvSlotSize = static_cast<UINT>(kDrawConstantSlotSize);
	const D3D12_GPU_VIRTUAL_ADDRESS DrawBufferGpuAddress = m_draw_constant_buffer_->GetGPUVirtualAddress();
	for (UINT SlotIndex = 0; SlotIndex < kMaxDrawConstantSlots; ++SlotIndex)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE SlotCpuHandle = DrawCbvHeapStartCpu;
		SlotCpuHandle.ptr += static_cast<SIZE_T>(SlotIndex) * m_cbv_descriptor_size_;

		D3D12_CONSTANT_BUFFER_VIEW_DESC DrawCbvDesc{};
		DrawCbvDesc.BufferLocation = DrawBufferGpuAddress + static_cast<UINT64>(SlotIndex) * kDrawConstantSlotSize;
		DrawCbvDesc.SizeInBytes = DrawCbvSlotSize;
		m_device_->CreateConstantBufferView(&DrawCbvDesc, SlotCpuHandle);

		D3D12_GPU_DESCRIPTOR_HANDLE SlotGpuHandle = DrawCbvHeapStartGpu;
		SlotGpuHandle.ptr += static_cast<UINT64>(SlotIndex) * m_cbv_descriptor_size_;
		m_draw_cbv_gpu_handles_[SlotIndex] = SlotGpuHandle;
	}

	DrawConstants* DrawSlot0 = reinterpret_cast<DrawConstants*>(m_draw_constants_cpu_);
	XMStoreFloat4x4(&DrawSlot0->world, XMMatrixIdentity());
	DrawSlot0->mesh_color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	return true;
}

bool Dx12Renderer::CreateUploadBuffer(const void* InData, UINT64 InSize, ID3D12Resource** OutUploadBuffer)
{
	UINT64 UnusedCapacity = 0;
	if (!EnsureUploadBufferCapacity(InSize, OutUploadBuffer, &UnusedCapacity))
	{
		return false;
	}

	return UploadBufferData(*OutUploadBuffer, InData, InSize);
}

bool Dx12Renderer::EnsureUploadBufferCapacity(
	UINT64 InRequiredSize,
	ID3D12Resource** InOutUploadBuffer,
	UINT64* InOutCapacity)
{
	if (InOutUploadBuffer == nullptr || InOutCapacity == nullptr || m_device_ == nullptr)
	{
		return false;
	}

	if (*InOutUploadBuffer != nullptr && *InOutCapacity >= InRequiredSize)
	{
		return true;
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> ExistingBuffer;
	if (*InOutUploadBuffer != nullptr)
	{
		ExistingBuffer.Attach(*InOutUploadBuffer);
		*InOutUploadBuffer = nullptr;
		RetireUploadBuffer(ExistingBuffer);
	}

	const UINT64 GrownCapacity = (*InOutCapacity > 0) ? (*InOutCapacity * 2) : InRequiredSize;
	const UINT64 TargetCapacity = (InRequiredSize > GrownCapacity) ? InRequiredSize : GrownCapacity;
	const UINT64 AllocatedSize = AlignTo(TargetCapacity, 65536);

	D3D12_HEAP_PROPERTIES HeapProps{};
	HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC BufferDesc{};
	BufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	BufferDesc.Width = AllocatedSize;
	BufferDesc.Height = 1;
	BufferDesc.DepthOrArraySize = 1;
	BufferDesc.MipLevels = 1;
	BufferDesc.SampleDesc.Count = 1;
	BufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	Microsoft::WRL::ComPtr<ID3D12Resource> NewBuffer;
	if (FAILED(m_device_->CreateCommittedResource(&HeapProps,
												  D3D12_HEAP_FLAG_NONE,
												  &BufferDesc,
												  D3D12_RESOURCE_STATE_GENERIC_READ,
												  nullptr,
												  IID_PPV_ARGS(&NewBuffer))))
	{
		return false;
	}

	*InOutUploadBuffer = NewBuffer.Detach();
	*InOutCapacity = AllocatedSize;
	return true;
}

bool Dx12Renderer::UploadBufferData(ID3D12Resource* InUploadBuffer, const void* InData, UINT64 InSize)
{
	if (InUploadBuffer == nullptr || InData == nullptr || InSize == 0)
	{
		return false;
	}

	void* Mapped = nullptr;
	if (FAILED(InUploadBuffer->Map(0, nullptr, &Mapped)))
	{
		return false;
	}
	std::memcpy(Mapped, InData, static_cast<size_t>(InSize));
	InUploadBuffer->Unmap(0, nullptr);
	return true;
}

bool Dx12Renderer::ExecuteUploadCopy(
	ID3D12Resource* InDestination,
	const void* InData,
	UINT64 InSize,
	D3D12_RESOURCE_STATES InFinalState)
{
	if (InDestination == nullptr || InData == nullptr || InSize == 0 || m_device_ == nullptr)
	{
		return false;
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> UploadBuffer;
	UINT64 UnusedCapacity = 0;
	if (!EnsureUploadBufferCapacity(InSize, UploadBuffer.ReleaseAndGetAddressOf(), &UnusedCapacity))
	{
		return false;
	}
	if (!UploadBufferData(UploadBuffer.Get(), InData, InSize))
	{
		return false;
	}

	(void)WaitForGpuWithTimeout(1000);

	ID3D12CommandAllocator* UploadAllocator = m_command_allocators_[0].Get();
	if (UploadAllocator == nullptr || m_command_list_ == nullptr)
	{
		return false;
	}

	if (FAILED(UploadAllocator->Reset()))
	{
		return false;
	}
	if (FAILED(m_command_list_->Reset(UploadAllocator, nullptr)))
	{
		return false;
	}

	m_command_list_->CopyResource(InDestination, UploadBuffer.Get());

	D3D12_RESOURCE_BARRIER Barrier{};
	Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	Barrier.Transition.pResource = InDestination;
	Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	Barrier.Transition.StateAfter = InFinalState;
	Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_command_list_->ResourceBarrier(1, &Barrier);

	if (FAILED(m_command_list_->Close()))
	{
		return false;
	}

	ID3D12CommandList* CommandLists[] = {m_command_list_.Get()};
	m_command_queue_->ExecuteCommandLists(1, CommandLists);

	const uint64_t FenceValue = m_fence_values_[m_frame_index_] + 1;
	if (FAILED(m_command_queue_->Signal(m_fence_.Get(), FenceValue)))
	{
		return false;
	}
	if (FAILED(m_fence_->SetEventOnCompletion(FenceValue, m_fence_event_)))
	{
		return false;
	}
	const DWORD WaitResult = WaitForSingleObject(m_fence_event_, 1000);
	if (WaitResult != WAIT_OBJECT_0)
	{
		return false;
	}

	m_fence_values_[m_frame_index_] = FenceValue + 1;
	m_last_submitted_fence_value_ = FenceValue;
	return true;
}

bool Dx12Renderer::CreateDefaultBufferWithInitialData(
	const void* InData,
	UINT64 InSize,
	D3D12_RESOURCE_STATES InFinalState,
	ID3D12Resource** OutResource)
{
	if (OutResource == nullptr || InData == nullptr || InSize == 0 || m_device_ == nullptr)
	{
		return false;
	}

	const UINT64 AlignedSize = AlignTo(InSize, 65536);

	D3D12_HEAP_PROPERTIES DefaultHeapProps{};
	DefaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC BufferDesc{};
	BufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	BufferDesc.Width = AlignedSize;
	BufferDesc.Height = 1;
	BufferDesc.DepthOrArraySize = 1;
	BufferDesc.MipLevels = 1;
	BufferDesc.SampleDesc.Count = 1;
	BufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	if (FAILED(m_device_->CreateCommittedResource(&DefaultHeapProps,
												  D3D12_HEAP_FLAG_NONE,
												  &BufferDesc,
												  D3D12_RESOURCE_STATE_COPY_DEST,
												  nullptr,
												  IID_PPV_ARGS(OutResource))))
	{
		return false;
	}

	return ExecuteUploadCopy(*OutResource, InData, InSize, InFinalState);
}

void Dx12Renderer::ClearGpuStaticMeshCache()
{
	m_gpu_static_meshes_.clear();
	m_static_scene_draws_.clear();
	m_static_render_cache_hash_ = 0;
}

void Dx12Renderer::ClearGpuSkeletalMeshCache()
{
	m_gpu_skeletal_meshes_.clear();
	m_skeletal_scene_draws_.clear();
	m_skeletal_scene_cache_hash_ = 0;
}

bool Dx12Renderer::BuildSkeletalMeshGpuData(USkeletalMesh* InSkeletalMesh, FGpuSkeletalMeshResources* OutResources)
{
	if (InSkeletalMesh == nullptr || OutResources == nullptr)
	{
		return false;
	}

	const std::vector<USkeletalMesh::FSkinVertex>& SkinVertices = InSkeletalMesh->GetSkinVertices();
	const std::vector<uint32_t>& SourceIndices = InSkeletalMesh->GetIndices();
	if (SkinVertices.empty() || SourceIndices.empty())
	{
		return false;
	}

	const DirectX::XMFLOAT4 MeshColor{0.5f, 0.72f, 0.95f, 1.0f};
	std::vector<Vertex> GpuVertices;
	GpuVertices.reserve(SkinVertices.size());
	for (const USkeletalMesh::FSkinVertex& SkinVertex : SkinVertices)
	{
		GpuVertices.push_back(ConvertSkeletalMeshVertexLocal(SkinVertex, MeshColor));
	}

	const UINT64 VertexBufferSize = static_cast<UINT64>(GpuVertices.size() * sizeof(Vertex));
	const UINT64 IndexBufferSize = static_cast<UINT64>(SourceIndices.size() * sizeof(uint32_t));
	if (!CreateDefaultBufferWithInitialData(
			GpuVertices.data(),
			VertexBufferSize,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			OutResources->Buffers.vertex_buffer.ReleaseAndGetAddressOf()))
	{
		return false;
	}
	if (!CreateDefaultBufferWithInitialData(
			SourceIndices.data(),
			IndexBufferSize,
			D3D12_RESOURCE_STATE_INDEX_BUFFER,
			OutResources->Buffers.index_buffer.ReleaseAndGetAddressOf()))
	{
		return false;
	}

	OutResources->Buffers.vertex_buffer_view.BufferLocation =
		OutResources->Buffers.vertex_buffer->GetGPUVirtualAddress();
	OutResources->Buffers.vertex_buffer_view.SizeInBytes = static_cast<UINT>(VertexBufferSize);
	OutResources->Buffers.vertex_buffer_view.StrideInBytes = sizeof(Vertex);
	OutResources->Buffers.index_buffer_view.BufferLocation =
		OutResources->Buffers.index_buffer->GetGPUVirtualAddress();
	OutResources->Buffers.index_buffer_view.SizeInBytes = static_cast<UINT>(IndexBufferSize);
	OutResources->Buffers.index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
	OutResources->Buffers.index_count = static_cast<UINT>(SourceIndices.size());
	return true;
}

bool Dx12Renderer::EnsureGpuSkeletalMeshResources(USkeletalMesh* InSkeletalMesh)
{
	if (InSkeletalMesh == nullptr || m_device_ == nullptr)
	{
		return false;
	}

	const auto ExistingIt = m_gpu_skeletal_meshes_.find(InSkeletalMesh);
	if (ExistingIt != m_gpu_skeletal_meshes_.end())
	{
		return ExistingIt->second.Buffers.index_count > 0;
	}

	FGpuSkeletalMeshResources NewResources;
	if (!BuildSkeletalMeshGpuData(InSkeletalMesh, &NewResources))
	{
		return false;
	}

	m_gpu_skeletal_meshes_.emplace(InSkeletalMesh, std::move(NewResources));
	return true;
}

bool Dx12Renderer::BuildStaticMeshGpuData(UStaticMesh* InStaticMesh, FGpuStaticMeshResources* OutResources)
{
	if (InStaticMesh == nullptr || OutResources == nullptr)
	{
		return false;
	}

	const std::vector<UStaticMesh::FVertex>& SourceVertices = InStaticMesh->GetVertices();
	const std::vector<uint32_t>& SourceIndices = InStaticMesh->GetIndices();
	if (SourceVertices.empty() || SourceIndices.empty())
	{
		return false;
	}

	std::vector<Vertex> GpuVertices;
	GpuVertices.reserve(SourceVertices.size());
	for (const UStaticMesh::FVertex& SourceVertex : SourceVertices)
	{
		Vertex GpuVertex{};
		GpuVertex.position = {SourceVertex.Position.X, SourceVertex.Position.Y, SourceVertex.Position.Z};
		GpuVertex.normal = {SourceVertex.Normal.X, SourceVertex.Normal.Y, SourceVertex.Normal.Z};
		GpuVertex.color = {0.88f, 0.82f, 0.72f, 1.0f};
		GpuVertices.push_back(GpuVertex);
	}

	const UINT64 VertexBufferSize = static_cast<UINT64>(GpuVertices.size() * sizeof(Vertex));
	const UINT64 IndexBufferSize = static_cast<UINT64>(SourceIndices.size() * sizeof(uint32_t));

	if (!CreateDefaultBufferWithInitialData(
			GpuVertices.data(),
			VertexBufferSize,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			OutResources->Buffers.vertex_buffer.ReleaseAndGetAddressOf()))
	{
		return false;
	}

	if (!CreateDefaultBufferWithInitialData(
			SourceIndices.data(),
			IndexBufferSize,
			D3D12_RESOURCE_STATE_INDEX_BUFFER,
			OutResources->Buffers.index_buffer.ReleaseAndGetAddressOf()))
	{
		return false;
	}

	OutResources->Buffers.vertex_buffer_view.BufferLocation =
		OutResources->Buffers.vertex_buffer->GetGPUVirtualAddress();
	OutResources->Buffers.vertex_buffer_view.SizeInBytes = static_cast<UINT>(VertexBufferSize);
	OutResources->Buffers.vertex_buffer_view.StrideInBytes = sizeof(Vertex);

	OutResources->Buffers.index_buffer_view.BufferLocation =
		OutResources->Buffers.index_buffer->GetGPUVirtualAddress();
	OutResources->Buffers.index_buffer_view.SizeInBytes = static_cast<UINT>(IndexBufferSize);
	OutResources->Buffers.index_buffer_view.Format = DXGI_FORMAT_R32_UINT;

	OutResources->Buffers.index_count = static_cast<UINT>(SourceIndices.size());
	return true;
}

bool Dx12Renderer::EnsureGpuStaticMeshResources(UStaticMesh* InStaticMesh)
{
	if (InStaticMesh == nullptr || m_device_ == nullptr)
	{
		return false;
	}

	const auto ExistingIt = m_gpu_static_meshes_.find(InStaticMesh);
	if (ExistingIt != m_gpu_static_meshes_.end())
	{
		return ExistingIt->second.Buffers.index_count > 0;
	}

	FGpuStaticMeshResources NewResources;
	if (!BuildStaticMeshGpuData(InStaticMesh, &NewResources))
	{
		return false;
	}

	m_gpu_static_meshes_.emplace(InStaticMesh, std::move(NewResources));
	return true;
}

namespace
{
	constexpr float kCameraFovDegrees = 60.0f;
	constexpr float kDebugGroundZ = 0.01f;
	constexpr float kDebugAxisAlpha = 0.5f;
	constexpr float kDebugAxisExtent = 500.0f;
	constexpr float kDebugGridMinorAlpha = 0.18f;
	constexpr float kDebugGridMajorAlpha = 0.32f;
	constexpr int kDebugGridMajorInterval = 10;
	constexpr float kWorldAxisPositiveAlpha = 0.95f;
	constexpr float kDebugGridHalfExtentMarginCells = 6.0f;
	constexpr float kDebugGridTargetLinesPerAxis = 40.0f;
	constexpr int kDebugGridMaxLinesPerAxis = 96;
	constexpr float kDebugGridFadeStartRatio = 0.82f;
	constexpr float kDebugGridFadeEndRatio = 1.02f;
	constexpr float kDebugGridMinHalfExtent = 40.0f;
	constexpr float kDebugGridFallbackHalfExtent = 500.0f;
	constexpr float kDebugGridHeightExtentScale = 14.0f;
	constexpr float kDebugGridHeightExtentBias = 48.0f;
	constexpr float kDebugGridObliqueExtentScale = 3.5f;
	constexpr float kDebugGridNearHorizontalPitchThreshold = 0.25f;
	constexpr float kDebugGridZFadeStartHeight = 50.0f;
	constexpr float kDebugGridZFadeEndHeight = 160.0f;
	constexpr float kDebugGridHeightBucketSize = 4.0f;

	constexpr float kGridCellSizeTiers[] = {
		1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f};

	float ComputeGridRadialFadeAlpha(float InDistance, float InHalfExtent, float InBaseAlpha)
	{
		if (InHalfExtent <= 0.0f || InBaseAlpha <= 0.0f)
		{
			return 0.0f;
		}

		const float FadeStart = InHalfExtent * kDebugGridFadeStartRatio;
		const float FadeEnd = InHalfExtent * kDebugGridFadeEndRatio;
		if (InDistance <= FadeStart)
		{
			return InBaseAlpha;
		}
		if (InDistance >= FadeEnd)
		{
			return 0.0f;
		}

		const float FadeT = (InDistance - FadeStart) / (FadeEnd - FadeStart);
		return InBaseAlpha * (1.0f - FadeT);
	}

	float ComputeGridHeightFadeMultiplier(float InHeightAboveGround)
	{
		if (InHeightAboveGround <= kDebugGridZFadeStartHeight)
		{
			return 1.0f;
		}
		if (InHeightAboveGround >= kDebugGridZFadeEndHeight)
		{
			return 0.0f;
		}

		const float FadeT =
			(InHeightAboveGround - kDebugGridZFadeStartHeight) /
			(kDebugGridZFadeEndHeight - kDebugGridZFadeStartHeight);
		return 1.0f - FadeT;
	}

	float ComputeGridFadeAlpha(float InDistance, float InHalfExtent, float InBaseAlpha, float InHeightAboveGround)
	{
		const float RadialAlpha = ComputeGridRadialFadeAlpha(InDistance, InHalfExtent, InBaseAlpha);
		return RadialAlpha * ComputeGridHeightFadeMultiplier(InHeightAboveGround);
	}

	int ComputeGridHeightBucket(float InHeightAboveGround)
	{
		return static_cast<int>(std::floor(InHeightAboveGround / kDebugGridHeightBucketSize));
	}

	float SelectGridCellSize(float InVisibleHalfExtent)
	{
		const float ClampedVisibleHalfExtent = std::max(InVisibleHalfExtent, kDebugGridMinHalfExtent);
		const float RawCellSize = ClampedVisibleHalfExtent / kDebugGridTargetLinesPerAxis;
		for (const float Tier : kGridCellSizeTiers)
		{
			if (Tier >= RawCellSize)
			{
				return Tier;
			}
		}
		return kGridCellSizeTiers[(sizeof(kGridCellSizeTiers) / sizeof(kGridCellSizeTiers[0])) - 1];
	}

	float ComputeVisibleGroundHalfExtent(
		const Dx12Renderer::CameraState& InCamera,
		float InAspect,
		float InFovYRadians)
	{
		const float ClampedPitch = std::clamp(InCamera.pitch, -1.4f, 1.4f);
		const float SinPitch = std::sin(ClampedPitch);
		const float ForwardZ = SinPitch;
		const float HeightAboveGround = std::max(InCamera.position.z - kDebugGroundZ, 0.5f);
		const float TanHalfFov = std::tan(InFovYRadians * 0.5f);
		const float AspectScale = std::max(InAspect, 1.0f);

		float Extent = std::max(
			kDebugGridMinHalfExtent,
			HeightAboveGround * TanHalfFov * AspectScale * kDebugGridHeightExtentScale + kDebugGridHeightExtentBias);

		if (std::abs(ForwardZ) < 1e-4f)
		{
			return std::max(
				Extent,
				std::max(
					kDebugGridFallbackHalfExtent * 0.75f,
					std::hypot(InCamera.position.x, InCamera.position.y) * 0.15f + 120.0f));
		}

		const float RayT = (kDebugGroundZ - InCamera.position.z) / ForwardZ;
		if (RayT > 0.0f)
		{
			const float CenterFootprint = RayT * TanHalfFov * AspectScale;
			Extent = std::max(Extent, CenterFootprint);

			if (ForwardZ < -0.05f)
			{
				const float PitchFromHorizontal = std::clamp(std::abs(ClampedPitch), 0.08f, 1.4f);
				const float GroundReach = HeightAboveGround / std::tan(PitchFromHorizontal);
				const float ObliqueFootprint = GroundReach * TanHalfFov * AspectScale;
				Extent = std::max(Extent, ObliqueFootprint * kDebugGridObliqueExtentScale);
			}
		}
		else
		{
			Extent = std::max(Extent, HeightAboveGround * 4.0f);
		}

		if (std::abs(ForwardZ) < kDebugGridNearHorizontalPitchThreshold)
		{
			Extent = std::max(Extent, kDebugGridFallbackHalfExtent * 0.75f);
		}

		return std::min(Extent, kDebugGridFallbackHalfExtent * 4.0f);
	}

	int ComputeGridHalfExtentBucket(float InHalfExtent, float InCellSize)
	{
		if (InCellSize <= 0.0f)
		{
			return 0;
		}
		return static_cast<int>(std::floor((InHalfExtent / InCellSize) / 4.0f));
	}
}

bool Dx12Renderer::CreateDebugAxisMesh()
{
	const XMFLOAT3 UpNormal{0.0f, 0.0f, 1.0f};
	const XMFLOAT4 AxisXColor{1.0f, 0.2f, 0.2f, kDebugAxisAlpha};
	const XMFLOAT4 AxisYColor{0.2f, 0.9f, 0.2f, kDebugAxisAlpha};
	const std::array<Vertex, 4> AxisLines = {{
		{{-kDebugAxisExtent, 0.0f, kDebugGroundZ}, UpNormal, AxisXColor},
		{{kDebugAxisExtent, 0.0f, kDebugGroundZ}, UpNormal, AxisXColor},
		{{0.0f, -kDebugAxisExtent, kDebugGroundZ}, UpNormal, AxisYColor},
		{{0.0f, kDebugAxisExtent, kDebugGroundZ}, UpNormal, AxisYColor},
	}};

	const UINT64 BufferSize = static_cast<UINT64>(AxisLines.size() * sizeof(Vertex));
	RetireUploadBuffer(m_debug_axis_mesh_.vertex_buffer);
	m_debug_axis_mesh_.vertex_buffer_view = {};
	m_debug_axis_mesh_.vertex_count = 0;
	if (!CreateUploadBuffer(AxisLines.data(), BufferSize, m_debug_axis_mesh_.vertex_buffer.ReleaseAndGetAddressOf()))
	{
		return false;
	}

	m_debug_axis_mesh_.vertex_buffer_view.BufferLocation = m_debug_axis_mesh_.vertex_buffer->GetGPUVirtualAddress();
	m_debug_axis_mesh_.vertex_buffer_view.SizeInBytes = static_cast<UINT>(BufferSize);
	m_debug_axis_mesh_.vertex_buffer_view.StrideInBytes = sizeof(Vertex);
	m_debug_axis_mesh_.vertex_count = static_cast<UINT>(AxisLines.size());
	return true;
}

bool Dx12Renderer::UpdateDebugGridMesh(const CameraState& InCamera)
{
	const float Aspect = (m_height_ > 0) ? (static_cast<float>(m_width_) / static_cast<float>(m_height_)) : 16.0f / 9.0f;
	const float FovYRadians = XMConvertToRadians(kCameraFovDegrees);
	const float VisibleHalfExtent = ComputeVisibleGroundHalfExtent(InCamera, Aspect, FovYRadians);
	const float CellSize = SelectGridCellSize(VisibleHalfExtent);
	const float GridHalfExtent = std::max(
		VisibleHalfExtent + (CellSize * kDebugGridHalfExtentMarginCells),
		kDebugGridMinHalfExtent);
	const float HeightAboveGround = std::max(InCamera.position.z - kDebugGroundZ, 0.0f);
	const float HeightFadeMultiplier = ComputeGridHeightFadeMultiplier(HeightAboveGround);
	if (HeightFadeMultiplier <= 0.0f)
	{
		RetireUploadBuffer(m_debug_line_mesh_.vertex_buffer);
		m_debug_line_mesh_.vertex_buffer_view = {};
		m_debug_line_mesh_.vertex_count = 0;
		m_bDebugGridMeshReady_ = true;
		m_debug_grid_snap_cell_x_ = static_cast<int>(std::floor(InCamera.position.x / CellSize));
		m_debug_grid_snap_cell_y_ = static_cast<int>(std::floor(InCamera.position.y / CellSize));
		m_debug_grid_cell_size_ = CellSize;
		m_debug_grid_half_extent_bucket_ = ComputeGridHalfExtentBucket(GridHalfExtent, CellSize);
		m_debug_grid_height_bucket_ = ComputeGridHeightBucket(HeightAboveGround);
		return false;
	}

	const int SnapCellX = static_cast<int>(std::floor(InCamera.position.x / CellSize));
	const int SnapCellY = static_cast<int>(std::floor(InCamera.position.y / CellSize));
	const int HalfExtentBucket = ComputeGridHalfExtentBucket(GridHalfExtent, CellSize);
	const int HeightBucket = ComputeGridHeightBucket(HeightAboveGround);
	if (m_bDebugGridMeshReady_ &&
		SnapCellX == m_debug_grid_snap_cell_x_ &&
		SnapCellY == m_debug_grid_snap_cell_y_ &&
		CellSize == m_debug_grid_cell_size_ &&
		HalfExtentBucket == m_debug_grid_half_extent_bucket_ &&
		HeightBucket == m_debug_grid_height_bucket_)
	{
		return m_debug_line_mesh_.vertex_count > 0;
	}

	int HalfLineCount = static_cast<int>(std::ceil(GridHalfExtent / CellSize));
	HalfLineCount = std::clamp(HalfLineCount, 1, kDebugGridMaxLinesPerAxis);
	std::vector<Vertex> DebugLines;
	DebugLines.reserve(static_cast<size_t>((HalfLineCount * 2 + 1) * 4 + 6));

	const float CenterX = static_cast<float>(SnapCellX) * CellSize;
	const float CenterY = static_cast<float>(SnapCellY) * CellSize;
	const float MinX = CenterX - GridHalfExtent;
	const float MaxX = CenterX + GridHalfExtent;
	const float MinY = CenterY - GridHalfExtent;
	const float MaxY = CenterY + GridHalfExtent;
	const XMFLOAT3 UpNormal{0.0f, 0.0f, 1.0f};
	const float CameraX = InCamera.position.x;
	const float CameraY = InCamera.position.y;

	auto AddGridLineSegment = [&](float InX0, float InY0, float InX1, float InY1, const XMFLOAT4& InBaseColor)
	{
		const float MidX = (InX0 + InX1) * 0.5f;
		const float MidY = (InY0 + InY1) * 0.5f;
		const float Distance = std::hypot(MidX - CameraX, MidY - CameraY);
		XMFLOAT4 FadedColor = InBaseColor;
		FadedColor.w = ComputeGridFadeAlpha(Distance, GridHalfExtent, InBaseColor.w, HeightAboveGround);
		DebugLines.push_back({{InX0, InY0, kDebugGroundZ}, UpNormal, FadedColor});
		DebugLines.push_back({{InX1, InY1, kDebugGroundZ}, UpNormal, FadedColor});
	};

	for (int LineIndex = -HalfLineCount; LineIndex <= HalfLineCount; ++LineIndex)
	{
		if (LineIndex == 0)
		{
			continue;
		}

		const bool bIsMajorLine = (LineIndex % kDebugGridMajorInterval) == 0;
		const XMFLOAT4 GridColor = bIsMajorLine ? XMFLOAT4{0.55f, 0.55f, 0.55f, kDebugGridMajorAlpha}
												  : XMFLOAT4{0.35f, 0.35f, 0.35f, kDebugGridMinorAlpha};
		const float Offset = static_cast<float>(LineIndex) * CellSize;
		const float LineY = CenterY + Offset;
		const float LineX = CenterX + Offset;
		AddGridLineSegment(MinX, LineY, MaxX, LineY, GridColor);
		AddGridLineSegment(LineX, MinY, LineX, MaxY, GridColor);
	}

	const float WorldAxisPositiveLength = CellSize * 0.5f;
	const float AxisAlpha = kWorldAxisPositiveAlpha * HeightFadeMultiplier;
	const XMFLOAT4 WorldAxisXColor{1.0f, 0.2f, 0.2f, AxisAlpha};
	const XMFLOAT4 WorldAxisYColor{0.2f, 0.9f, 0.2f, AxisAlpha};
	const XMFLOAT4 WorldAxisZColor{0.25f, 0.55f, 1.0f, AxisAlpha};
	DebugLines.push_back({{0.0f, 0.0f, 0.0f}, UpNormal, WorldAxisXColor});
	DebugLines.push_back({{WorldAxisPositiveLength, 0.0f, 0.0f}, UpNormal, WorldAxisXColor});
	DebugLines.push_back({{0.0f, 0.0f, 0.0f}, UpNormal, WorldAxisYColor});
	DebugLines.push_back({{0.0f, WorldAxisPositiveLength, 0.0f}, UpNormal, WorldAxisYColor});
	DebugLines.push_back({{0.0f, 0.0f, 0.0f}, UpNormal, WorldAxisZColor});
	DebugLines.push_back({{0.0f, 0.0f, WorldAxisPositiveLength}, UpNormal, WorldAxisZColor});

	if (DebugLines.empty())
	{
		return false;
	}

	const UINT64 BufferSize = static_cast<UINT64>(DebugLines.size() * sizeof(Vertex));
	RetireUploadBuffer(m_debug_line_mesh_.vertex_buffer);
	m_debug_line_mesh_.vertex_buffer_view = {};
	m_debug_line_mesh_.vertex_count = 0;
	if (!CreateUploadBuffer(DebugLines.data(), BufferSize, m_debug_line_mesh_.vertex_buffer.ReleaseAndGetAddressOf()))
	{
		m_bDebugGridMeshReady_ = false;
		return false;
	}

	m_debug_line_mesh_.vertex_buffer_view.BufferLocation = m_debug_line_mesh_.vertex_buffer->GetGPUVirtualAddress();
	m_debug_line_mesh_.vertex_buffer_view.SizeInBytes = static_cast<UINT>(BufferSize);
	m_debug_line_mesh_.vertex_buffer_view.StrideInBytes = sizeof(Vertex);
	m_debug_line_mesh_.vertex_count = static_cast<UINT>(DebugLines.size());
	m_debug_grid_snap_cell_x_ = SnapCellX;
	m_debug_grid_snap_cell_y_ = SnapCellY;
	m_debug_grid_cell_size_ = CellSize;
	m_debug_grid_half_extent_bucket_ = HalfExtentBucket;
	m_debug_grid_height_bucket_ = HeightBucket;
	m_bDebugGridMeshReady_ = true;
	return true;
}

bool Dx12Renderer::CreateMeshBuffers()
{
	if (!CreateDebugAxisMesh())
	{
		return false;
	}

	CameraState InitialCamera{};
	if (!UpdateDebugGridMesh(InitialCamera))
	{
		return false;
	}

	return true;
}

void Dx12Renderer::SetCameraClipDistances(float InNearPlane, float InFarPlane)
{
	m_near_clip_plane_ = std::max(InNearPlane, 0.001f);
	m_far_clip_plane_ = std::max(InFarPlane, m_near_clip_plane_ + 0.001f);
}

float Dx12Renderer::GetNearClipPlane() const
{
	return m_near_clip_plane_;
}

float Dx12Renderer::GetFarClipPlane() const
{
	return m_far_clip_plane_;
}

void Dx12Renderer::SetEditorGizmoMesh(const std::vector<Vertex>& InVertices, bool bTriangleList)
{
	m_editor_gizmo_triangle_list_ = bTriangleList;
	RetireUploadBuffer(m_editor_gizmo_mesh_.vertex_buffer);
	m_editor_gizmo_mesh_.vertex_buffer_view = {};
	m_editor_gizmo_mesh_.vertex_count = 0;
	if (InVertices.empty())
	{
		return;
	}

	const UINT64 BufferSize = static_cast<UINT64>(InVertices.size() * sizeof(Vertex));
	if (!CreateUploadBuffer(InVertices.data(), BufferSize, m_editor_gizmo_mesh_.vertex_buffer.ReleaseAndGetAddressOf()))
	{
		return;
	}

	m_editor_gizmo_mesh_.vertex_buffer_view.BufferLocation = m_editor_gizmo_mesh_.vertex_buffer->GetGPUVirtualAddress();
	m_editor_gizmo_mesh_.vertex_buffer_view.SizeInBytes = static_cast<UINT>(BufferSize);
	m_editor_gizmo_mesh_.vertex_buffer_view.StrideInBytes = sizeof(Vertex);
	m_editor_gizmo_mesh_.vertex_count = static_cast<UINT>(InVertices.size());
}

void Dx12Renderer::SetEditorOverlayLines(const std::vector<Vertex>& InVertices)
{
	RetireUploadBuffer(m_editor_overlay_line_mesh_.vertex_buffer);
	m_editor_overlay_line_mesh_.vertex_buffer_view = {};
	m_editor_overlay_line_mesh_.vertex_count = 0;
	if (InVertices.empty())
	{
		return;
	}

	const UINT64 BufferSize = static_cast<UINT64>(InVertices.size() * sizeof(Vertex));
	if (!CreateUploadBuffer(InVertices.data(), BufferSize, m_editor_overlay_line_mesh_.vertex_buffer.ReleaseAndGetAddressOf()))
	{
		return;
	}

	m_editor_overlay_line_mesh_.vertex_buffer_view.BufferLocation =
		m_editor_overlay_line_mesh_.vertex_buffer->GetGPUVirtualAddress();
	m_editor_overlay_line_mesh_.vertex_buffer_view.SizeInBytes = static_cast<UINT>(BufferSize);
	m_editor_overlay_line_mesh_.vertex_buffer_view.StrideInBytes = sizeof(Vertex);
	m_editor_overlay_line_mesh_.vertex_count = static_cast<UINT>(InVertices.size());
}

void Dx12Renderer::UpdateCameraConstants(const CameraState& InCamera)
{
	const FEditorViewportMatrices Matrices = FEditorViewMatrices::Build(
		InCamera,
		m_width_,
		m_height_,
		m_near_clip_plane_,
		m_far_clip_plane_);
	XMStoreFloat4x4(&m_scene_constants_cpu_->view_projection, Matrices.ViewProjection);
	m_scene_constants_cpu_->light_direction = XMFLOAT3(-0.45f, -0.3f, -1.0f);
	m_scene_constants_cpu_->ambient_strength = 0.35f;
}

void Dx12Renderer::RetireUploadBuffer(Microsoft::WRL::ComPtr<ID3D12Resource>& InOutResource)
{
	if (InOutResource == nullptr)
	{
		return;
	}

	if (m_fence_ == nullptr || m_last_submitted_fence_value_ == 0)
	{
		InOutResource.Reset();
		return;
	}

	FRetiredUploadBuffer RetiredBuffer;
	RetiredBuffer.Resource = std::move(InOutResource);
	RetiredBuffer.FenceValue = m_last_submitted_fence_value_;
	m_retired_upload_buffers_.push_back(std::move(RetiredBuffer));
}

void Dx12Renderer::ReleaseCompletedUploadBuffers()
{
	if (m_fence_ == nullptr || m_retired_upload_buffers_.empty())
	{
		return;
	}

	const uint64_t CompletedValue = m_fence_->GetCompletedValue();
	m_retired_upload_buffers_.erase(
		std::remove_if(
			m_retired_upload_buffers_.begin(),
			m_retired_upload_buffers_.end(),
			[CompletedValue](const FRetiredUploadBuffer& InBuffer)
			{
				return InBuffer.FenceValue <= CompletedValue;
			}),
		m_retired_upload_buffers_.end());
}

void Dx12Renderer::Render(const CameraState& InCamera)
{
	if (m_device_lost_)
	{
		return;
	}
	if (m_width_ == 0 || m_height_ == 0 || m_root_signature_ == nullptr)
	{
		return;
	}
	if (m_render_targets_[m_frame_index_] == nullptr || m_depth_stencil_ == nullptr)
	{
		return;
	}

	ReleaseCompletedUploadBuffers();
	UpdateCameraConstants(InCamera);
	UpdateDebugGridMesh(InCamera);

	auto* Allocator = m_command_allocators_[m_frame_index_].Get();
	if (FAILED(Allocator->Reset()))
	{
		return;
	}
	if (FAILED(m_command_list_->Reset(Allocator, m_triangle_pipeline_state_.Get())))
	{
		return;
	}

	D3D12_RESOURCE_BARRIER BeginBarrier{};
	BeginBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	BeginBarrier.Transition.pResource = m_render_targets_[m_frame_index_].Get();
	BeginBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	BeginBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	BeginBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_command_list_->ResourceBarrier(1, &BeginBarrier);

	D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle = m_rtv_heap_->GetCPUDescriptorHandleForHeapStart();
	RtvHandle.ptr += static_cast<SIZE_T>(m_frame_index_) * m_rtv_descriptor_size_;
	D3D12_CPU_DESCRIPTOR_HANDLE DsvHandle = m_dsv_heap_->GetCPUDescriptorHandleForHeapStart();

	constexpr float kClearColor[4] = {0.05f, 0.1f, 0.18f, 1.0f};
	m_command_list_->RSSetViewports(1, &m_viewport_);
	m_command_list_->RSSetScissorRects(1, &m_scissor_rect_);
	m_command_list_->OMSetRenderTargets(1, &RtvHandle, FALSE, &DsvHandle);
	m_command_list_->ClearRenderTargetView(RtvHandle, kClearColor, 0, nullptr);
	m_command_list_->ClearDepthStencilView(DsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	ID3D12DescriptorHeap* DescriptorHeaps[] = {m_cbv_heap_.Get()};
	m_command_list_->SetDescriptorHeaps(1, DescriptorHeaps);
	m_command_list_->SetGraphicsRootSignature(m_root_signature_.Get());
	const D3D12_GPU_DESCRIPTOR_HANDLE SceneCbvGpuHandle = m_cbv_heap_->GetGPUDescriptorHandleForHeapStart();
	const D3D12_GPU_DESCRIPTOR_HANDLE DefaultDrawCbvGpuHandle = m_draw_cbv_gpu_handles_[0];

	auto WriteDrawSlot = [this](UINT InSlotIndex, const DirectX::XMFLOAT4X4& InWorldMatrix, const DirectX::XMFLOAT4& InMeshColor)
	{
		if (InSlotIndex >= kMaxDrawConstantSlots || m_draw_constants_cpu_ == nullptr)
		{
			return;
		}

		auto* DrawSlot = reinterpret_cast<DrawConstants*>(
			reinterpret_cast<uint8_t*>(m_draw_constants_cpu_) + static_cast<size_t>(InSlotIndex) * kDrawConstantSlotSize);
		DrawSlot->world = InWorldMatrix;
		DrawSlot->mesh_color = InMeshColor;
	};

	DirectX::XMFLOAT4X4 IdentityWorldMatrix{};
	XMStoreFloat4x4(&IdentityWorldMatrix, XMMatrixIdentity());
	WriteDrawSlot(0, IdentityWorldMatrix, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));

	m_command_list_->SetGraphicsRootDescriptorTable(0, SceneCbvGpuHandle);
	m_command_list_->SetGraphicsRootDescriptorTable(1, DefaultDrawCbvGpuHandle);

	UINT NextDrawSlotIndex = 1;

	if (!m_static_scene_draws_.empty())
	{
		m_command_list_->SetPipelineState(m_triangle_pipeline_state_.Get());
		m_command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		UINT DrawSlotIndex = NextDrawSlotIndex;
		for (const FSceneStaticDrawItem& DrawItem : m_static_scene_draws_)
		{
			if (DrawItem.StaticMesh == nullptr || DrawSlotIndex >= kMaxDrawConstantSlots)
			{
				continue;
			}

			const auto GpuMeshIt = m_gpu_static_meshes_.find(DrawItem.StaticMesh);
			if (GpuMeshIt == m_gpu_static_meshes_.end() || GpuMeshIt->second.Buffers.index_count == 0)
			{
				continue;
			}

			FTransform WorldTransform = DrawItem.WorldTransform;
			if (DrawItem.PrimitiveComponent != nullptr)
			{
				WorldTransform = DrawItem.PrimitiveComponent->GetWorldTransform();
			}

			DirectX::XMFLOAT4X4 WorldMatrix{};
			XMStoreFloat4x4(&WorldMatrix, WorldTransform.ToMatrix());
			WriteDrawSlot(DrawSlotIndex, WorldMatrix, DrawItem.MeshColor);
			const FIndexedMeshBuffers& MeshBuffers = GpuMeshIt->second.Buffers;

			m_command_list_->SetGraphicsRootDescriptorTable(0, SceneCbvGpuHandle);
			m_command_list_->SetGraphicsRootDescriptorTable(1, m_draw_cbv_gpu_handles_[DrawSlotIndex]);
			m_command_list_->IASetVertexBuffers(0, 1, &MeshBuffers.vertex_buffer_view);
			m_command_list_->IASetIndexBuffer(&MeshBuffers.index_buffer_view);
			m_command_list_->DrawIndexedInstanced(MeshBuffers.index_count, 1, 0, 0, 0);
			++DrawSlotIndex;
		}
		NextDrawSlotIndex = DrawSlotIndex;
	}

	if (!m_skeletal_scene_draws_.empty())
	{
		m_command_list_->SetPipelineState(m_triangle_pipeline_state_.Get());
		m_command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		UINT DrawSlotIndex = NextDrawSlotIndex;
		for (const FSceneSkeletalDrawItem& DrawItem : m_skeletal_scene_draws_)
		{
			if (DrawItem.SkeletalMesh == nullptr || DrawSlotIndex >= kMaxDrawConstantSlots)
			{
				continue;
			}

			const auto GpuMeshIt = m_gpu_skeletal_meshes_.find(DrawItem.SkeletalMesh);
			if (GpuMeshIt == m_gpu_skeletal_meshes_.end() || GpuMeshIt->second.Buffers.index_count == 0)
			{
				continue;
			}

			FTransform WorldTransform = DrawItem.WorldTransform;
			if (DrawItem.PrimitiveComponent != nullptr)
			{
				WorldTransform = DrawItem.PrimitiveComponent->GetWorldTransform();
			}

			DirectX::XMFLOAT4X4 WorldMatrix{};
			XMStoreFloat4x4(&WorldMatrix, WorldTransform.ToMatrix());
			WriteDrawSlot(DrawSlotIndex, WorldMatrix, DrawItem.MeshColor);

			const FIndexedMeshBuffers& MeshBuffers = GpuMeshIt->second.Buffers;
			m_command_list_->SetGraphicsRootDescriptorTable(0, SceneCbvGpuHandle);
			m_command_list_->SetGraphicsRootDescriptorTable(1, m_draw_cbv_gpu_handles_[DrawSlotIndex]);
			m_command_list_->IASetVertexBuffers(0, 1, &MeshBuffers.vertex_buffer_view);
			m_command_list_->IASetIndexBuffer(&MeshBuffers.index_buffer_view);
			m_command_list_->DrawIndexedInstanced(MeshBuffers.index_count, 1, 0, 0, 0);
			++DrawSlotIndex;
		}
		NextDrawSlotIndex = DrawSlotIndex;
	}

	if (m_debug_line_mesh_.vertex_count > 0 || m_debug_axis_mesh_.vertex_count > 0)
	{
		DirectX::XMFLOAT4X4 IdentityWorldMatrix{};
		XMStoreFloat4x4(&IdentityWorldMatrix, XMMatrixIdentity());
		WriteDrawSlot(0, IdentityWorldMatrix, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		m_command_list_->SetGraphicsRootDescriptorTable(0, SceneCbvGpuHandle);
		m_command_list_->SetGraphicsRootDescriptorTable(1, DefaultDrawCbvGpuHandle);
		m_command_list_->SetPipelineState(m_line_pipeline_state_.Get());
		m_command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
		if (m_debug_line_mesh_.vertex_count > 0)
		{
			m_command_list_->IASetVertexBuffers(0, 1, &m_debug_line_mesh_.vertex_buffer_view);
			m_command_list_->DrawInstanced(m_debug_line_mesh_.vertex_count, 1, 0, 0);
		}
		if (m_debug_axis_mesh_.vertex_count > 0)
		{
			m_command_list_->IASetVertexBuffers(0, 1, &m_debug_axis_mesh_.vertex_buffer_view);
			m_command_list_->DrawInstanced(m_debug_axis_mesh_.vertex_count, 1, 0, 0);
		}
	}

	if (m_editor_gizmo_mesh_.vertex_count > 0)
	{
		DirectX::XMFLOAT4X4 IdentityWorldMatrix{};
		XMStoreFloat4x4(&IdentityWorldMatrix, XMMatrixIdentity());
		WriteDrawSlot(0, IdentityWorldMatrix, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		m_command_list_->SetGraphicsRootDescriptorTable(0, SceneCbvGpuHandle);
		m_command_list_->SetGraphicsRootDescriptorTable(1, DefaultDrawCbvGpuHandle);
		m_command_list_->SetPipelineState(
			m_editor_gizmo_triangle_list_
				? m_editor_gizmo_triangle_pipeline_state_.Get()
				: m_line_pipeline_state_.Get());
		m_command_list_->IASetPrimitiveTopology(
			m_editor_gizmo_triangle_list_ ? D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST : D3D_PRIMITIVE_TOPOLOGY_LINELIST);
		m_command_list_->IASetVertexBuffers(0, 1, &m_editor_gizmo_mesh_.vertex_buffer_view);
		m_command_list_->DrawInstanced(m_editor_gizmo_mesh_.vertex_count, 1, 0, 0);
	}

	if (m_editor_overlay_line_mesh_.vertex_count > 0)
	{
		DirectX::XMFLOAT4X4 IdentityWorldMatrix{};
		XMStoreFloat4x4(&IdentityWorldMatrix, XMMatrixIdentity());
		WriteDrawSlot(0, IdentityWorldMatrix, DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		m_command_list_->SetGraphicsRootDescriptorTable(0, SceneCbvGpuHandle);
		m_command_list_->SetGraphicsRootDescriptorTable(1, DefaultDrawCbvGpuHandle);
		m_command_list_->SetPipelineState(m_line_pipeline_state_.Get());
		m_command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
		m_command_list_->IASetVertexBuffers(0, 1, &m_editor_overlay_line_mesh_.vertex_buffer_view);
		m_command_list_->DrawInstanced(m_editor_overlay_line_mesh_.vertex_count, 1, 0, 0);
	}

	D3D12_RESOURCE_BARRIER EndBarrier{};
	EndBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	EndBarrier.Transition.pResource = m_render_targets_[m_frame_index_].Get();
	EndBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	EndBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	EndBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_command_list_->ResourceBarrier(1, &EndBarrier);

	if (FAILED(m_command_list_->Close()))
	{
		return;
	}

	ID3D12CommandList* CommandLists[] = {m_command_list_.Get()};
	m_command_queue_->ExecuteCommandLists(1, CommandLists);

	const HRESULT PresentHr = m_swap_chain_->Present(1, 0);
	if (FAILED(PresentHr))
	{
		if (IsDeviceRemovedHr(PresentHr))
		{
			m_device_lost_ = true;
		}
		return;
	}

	MoveToNextFrame();
}

void Dx12Renderer::MoveToNextFrame()
{
	const uint64_t CurrentFenceValue = m_fence_values_[m_frame_index_];
	const HRESULT SignalHr = m_command_queue_->Signal(m_fence_.Get(), CurrentFenceValue);
	if (FAILED(SignalHr))
	{
		if (IsDeviceRemovedHr(SignalHr))
		{
			m_device_lost_ = true;
		}
		return;
	}
	m_last_submitted_fence_value_ = CurrentFenceValue;

	m_frame_index_ = m_swap_chain_->GetCurrentBackBufferIndex();

	const uint64_t CompletedValue = m_fence_->GetCompletedValue();
	if (CompletedValue < m_fence_values_[m_frame_index_])
	{
		if (FAILED(m_fence_->SetEventOnCompletion(m_fence_values_[m_frame_index_], m_fence_event_)))
		{
			return;
		}
		const DWORD WaitResult = WaitForSingleObject(m_fence_event_, 100);
		if (WaitResult == WAIT_TIMEOUT)
		{
			// Transient GPU back pressure is not a device-lost condition.
			// Skip this frame and let the next tick retry naturally.
			return;
		}
		if (WaitResult != WAIT_OBJECT_0)
		{
			m_device_lost_ = true;
			return;
		}
	}

	m_fence_values_[m_frame_index_] = CurrentFenceValue + 1;
	ReleaseCompletedUploadBuffers();
}

void Dx12Renderer::WaitForGpu()
{
	if (m_command_queue_ == nullptr || m_fence_ == nullptr || m_fence_event_ == nullptr)
	{
		return;
	}

	const uint64_t FenceValue = m_fence_values_[m_frame_index_];
	if (FAILED(m_command_queue_->Signal(m_fence_.Get(), FenceValue)))
	{
		return;
	}
	m_last_submitted_fence_value_ = FenceValue;
	if (FAILED(m_fence_->SetEventOnCompletion(FenceValue, m_fence_event_)))
	{
		return;
	}
	const DWORD WaitResult = WaitForSingleObject(m_fence_event_, 100);
	if (WaitResult != WAIT_OBJECT_0)
	{
		return;
	}
	m_fence_values_[m_frame_index_] = FenceValue + 1;
	ReleaseCompletedUploadBuffers();
}

bool Dx12Renderer::WaitForGpuWithTimeout(DWORD InTimeoutMs)
{
	if (m_command_queue_ == nullptr || m_fence_ == nullptr || m_fence_event_ == nullptr)
	{
		return false;
	}

	const uint64_t FenceValue = m_fence_values_[m_frame_index_];
	if (FAILED(m_command_queue_->Signal(m_fence_.Get(), FenceValue)))
	{
		return false;
	}
	m_last_submitted_fence_value_ = FenceValue;
	if (FAILED(m_fence_->SetEventOnCompletion(FenceValue, m_fence_event_)))
	{
		return false;
	}

	const DWORD WaitResult = WaitForSingleObject(m_fence_event_, InTimeoutMs);
	if (WaitResult != WAIT_OBJECT_0)
	{
		return false;
	}

	m_fence_values_[m_frame_index_] = FenceValue + 1;
	ReleaseCompletedUploadBuffers();
	return true;
}

bool Dx12Renderer::CreateRenderTargets()
{
	D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle = m_rtv_heap_->GetCPUDescriptorHandleForHeapStart();
	for (UINT Frame = 0; Frame < kFrameCount; ++Frame)
	{
		if (FAILED(m_swap_chain_->GetBuffer(Frame, IID_PPV_ARGS(&m_render_targets_[Frame]))))
		{
			return false;
		}
		m_device_->CreateRenderTargetView(m_render_targets_[Frame].Get(), nullptr, RtvHandle);
		RtvHandle.ptr += static_cast<SIZE_T>(m_rtv_descriptor_size_);
	}
	return true;
}

bool Dx12Renderer::CreateDepthStencil()
{
	D3D12_CLEAR_VALUE DepthClear{};
	DepthClear.Format = DXGI_FORMAT_D32_FLOAT;
	DepthClear.DepthStencil.Depth = 1.0f;
	DepthClear.DepthStencil.Stencil = 0;

	D3D12_HEAP_PROPERTIES HeapProps{};
	HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC DepthDesc{};
	DepthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	DepthDesc.Width = m_width_;
	DepthDesc.Height = m_height_;
	DepthDesc.DepthOrArraySize = 1;
	DepthDesc.MipLevels = 1;
	DepthDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	DepthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	if (FAILED(m_device_->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &DepthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &DepthClear, IID_PPV_ARGS(&m_depth_stencil_))))
	{
		return false;
	}

	D3D12_DEPTH_STENCIL_VIEW_DESC DsvDesc{};
	DsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	DsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	m_device_->CreateDepthStencilView(m_depth_stencil_.Get(), &DsvDesc, m_dsv_heap_->GetCPUDescriptorHandleForHeapStart());
	return true;
}
