#include "render/Dx12Renderer.h"

#include "render/asset/Texture2D.h"
#include "render/asset/TextureResource.h"

namespace
{
class FakeD3D12Resource final : public ID3D12Resource
{
public:
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID InRiid, void** OutObject) override
	{
		if (OutObject == nullptr)
		{
			return E_POINTER;
		}

		*OutObject = nullptr;
		if (InRiid == IID_IUnknown || InRiid == __uuidof(ID3D12Resource))
		{
			AddRef();
			*OutObject = static_cast<ID3D12Resource*>(this);
			return S_OK;
		}

		return E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return ++RefCount_;
	}

	ULONG STDMETHODCALLTYPE Release() override
	{
		const ULONG Remaining = --RefCount_;
		if (Remaining == 0)
		{
			delete this;
		}
		return Remaining;
	}

	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT*, void*) override
	{
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void*) override
	{
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown*) override
	{
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE SetName(LPCWSTR) override
	{
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetDevice(REFIID, void**) override
	{
		return E_NOTIMPL;
	}

	D3D12_RESOURCE_DESC STDMETHODCALLTYPE GetDesc() override
	{
		D3D12_RESOURCE_DESC Desc{};
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		Desc.Width = 1;
		Desc.Height = 1;
		Desc.DepthOrArraySize = 1;
		Desc.MipLevels = 1;
		Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		Desc.SampleDesc.Count = 1;
		Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		return Desc;
	}

	D3D12_GPU_VIRTUAL_ADDRESS STDMETHODCALLTYPE GetGPUVirtualAddress() override
	{
		return 0;
	}

	HRESULT STDMETHODCALLTYPE Map(
		UINT,
		const D3D12_RANGE*,
		void**) override
	{
		return E_NOTIMPL;
	}

	void STDMETHODCALLTYPE Unmap(UINT, const D3D12_RANGE*) override
	{
	}

	HRESULT STDMETHODCALLTYPE WriteToSubresource(
		UINT,
		const D3D12_BOX*,
		const void*,
		UINT,
		UINT) override
	{
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE ReadFromSubresource(
		void*,
		UINT,
		UINT,
		UINT,
		const D3D12_BOX*) override
	{
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE GetHeapProperties(D3D12_HEAP_PROPERTIES*, D3D12_HEAP_FLAGS*) override
	{
		return E_NOTIMPL;
	}

private:
	ULONG RefCount_ = 1;
};
} // namespace

Dx12Renderer::Dx12Renderer() = default;

Dx12Renderer::~Dx12Renderer() = default;

void Dx12Renderer::Render(const void* InRenderCommands, size_t InCommandCount)
{
	(void)InRenderCommands;
	(void)InCommandCount;
}

bool Dx12Renderer::UploadTexture2DResource(UTexture2D* InTexture, FTextureResource* OutResource)
{
	if (InTexture == nullptr || OutResource == nullptr || !InTexture->HasResidentPlatformData())
	{
		return false;
	}

	auto* FakeResource = new FakeD3D12Resource();
	FakeResource->AddRef();
	OutResource->GpuTexture = FakeResource;
	OutResource->GpuSrvHandle = 1;
	return true;
}
