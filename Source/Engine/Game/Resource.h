#pragma once
#include <cstdint>
#include <d3d11.h>

enum class ResourceType
{
    Structured,
    Index,
    Constant,
    Texture,
    Null
};

struct Resource
{
    ID3D11Resource* resource = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    ResourceType type = ResourceType::Null;
    uint32_t stride = 0;
    uint32_t count = 0;
};