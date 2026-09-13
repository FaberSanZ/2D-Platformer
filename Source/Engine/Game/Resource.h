#pragma once
#include <cstdint>
#include <d3d12.h>

struct Resource
{
    ID3D12Resource* resource = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
    uint32_t stride = 0;
    uint32_t count = 0;
};
