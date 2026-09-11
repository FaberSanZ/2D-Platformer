#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <vector>
#include "Resource.h"

struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT2 uv;

    DirectX::XMUINT4 joints;
    DirectX::XMFLOAT4 weights;
};

struct Node
{
    DirectX::XMFLOAT3 translation{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };

    DirectX::XMFLOAT3 baseTranslation{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 baseRotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 baseScale{ 1.0f, 1.0f, 1.0f };

    int32_t parent = -1;

    std::vector<uint32_t> children;

    int32_t meshIndex = -1;
    int32_t skinIndex = -1;
};

struct Skin
{
    std::vector<uint32_t> joints;
    std::vector<DirectX::XMFLOAT4X4> inverseBindMatrices;
};

enum class AnimationPath
{
    Translation,
    Rotation,
    Scale
};

enum class AnimationInterpolation
{
    Linear,
    Step
};

struct AnimationSampler
{
    std::vector<float> times;
    std::vector<DirectX::XMFLOAT4> values;

    AnimationInterpolation interpolation = AnimationInterpolation::Linear;
};

struct AnimationChannel
{
    uint32_t samplerIndex = 0;
    uint32_t nodeIndex = 0;

    AnimationPath path = AnimationPath::Translation;
};

struct AnimationClip
{
    std::string name;

    float duration = 0.0f;

    std::vector<AnimationSampler> samplers;
    std::vector<AnimationChannel> channels;
};

struct Material
{
    DirectX::XMFLOAT4 baseColor{ 1.0f, 1.0f, 1.0f, 1.0f };
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

struct NodePose
{
    DirectX::XMFLOAT3 translation{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };
};

struct Model
{
    std::vector<Node> nodes;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Skin> skins;
    std::vector<AnimationClip> animations;
};