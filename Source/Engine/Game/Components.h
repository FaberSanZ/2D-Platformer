#pragma once

#include <DirectXMath.h>

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

struct Model;

struct Model;

struct ModelComponent
{
    Model* model = nullptr;
    DirectX::XMFLOAT4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
};

