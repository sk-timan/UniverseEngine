#include "render/asset/TextureResource.h"

#include <d3d12.h>
#include <wrl/client.h>

void FTextureResource::Reset()
{
	if (GpuTexture != nullptr)
	{
		auto* Resource = static_cast<ID3D12Resource*>(GpuTexture);
		Resource->Release();
		GpuTexture = nullptr;
	}
	GpuSrvHandle = 0;
}
