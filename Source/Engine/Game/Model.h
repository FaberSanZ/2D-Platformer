#pragma once
#include <DirectXMath.h>
#include <vector>
#include "Resource.h"

struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT2 uv;
};



struct Node
{
    DirectX::XMFLOAT4X4 transform;
};


struct Material
{
};


struct MeshPart
{
    Resource vertex;
    Resource index;
    Node* node = nullptr;
    Material* material = nullptr;
};

struct Mesh
{
    std::vector<MeshPart> parts;
};

struct Model
{
    std::vector<Node> nodes;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
};
