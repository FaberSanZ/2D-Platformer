#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <DirectXMath.h>

using EntityID = uint64_t;

inline EntityID GenerateEntityID()
{
    static std::random_device randomDevice;
    static std::mt19937_64 generator(randomDevice());
    static std::uniform_int_distribution<EntityID> distribution;

    EntityID id = 0;

    while (id == 0)
        id = distribution(generator);

    return id;
}

struct IDComponent
{
    EntityID id = 0;
};

struct NameComponent
{
    std::string name;
};

struct TransformComponent
{
    DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };
};

enum class BodyType
{
    Static,
    Kinematic,
    Dynamic
};

struct RigidBodyComponent
{
    BodyType type = BodyType::Dynamic;
    DirectX::XMFLOAT3 linearVelocity{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 linearAcceleration{ 0.0f, 0.0f, 0.0f };
};

struct CameraComponent
{
    float fieldOfView = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
};

struct PrimaryCameraComponent
{
};

struct PlayerComponent
{
};

struct BossComponent
{
};

struct Model;

struct ModelComponent
{
    Model* model = nullptr;
    std::string assetPath;
    DirectX::XMFLOAT4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
};