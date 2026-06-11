#pragma once

#include <cstdint>

struct FTextureResource
{
	void* GpuTexture = nullptr;
	uint64_t GpuSrvHandle = 0;

	bool IsValid() const
	{
		return GpuTexture != nullptr && GpuSrvHandle != 0;
	}

	void Reset();
};
