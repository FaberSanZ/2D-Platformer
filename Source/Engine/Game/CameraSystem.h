#pragma once
#include <entt/entt.hpp>
#include "Components.h"

class CameraSystem
{
public:
    DirectX::XMMATRIX GetViewProjection(entt::registry& registry, float aspectRatio)
    {
        auto view = registry.view<TransformComponent, CameraComponent, PrimaryCameraComponent>();

        for (auto [entity, transform, camera] : view.each())
        {
            DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&transform.position);
            DirectX::XMVECTOR rotation = DirectX::XMLoadFloat4(&transform.rotation);

            DirectX::XMVECTOR forward = DirectX::XMVector3Rotate(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotation);
            DirectX::XMVECTOR up = DirectX::XMVector3Rotate(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotation);

            DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixLookToLH(position, forward, up);
            DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(camera.fieldOfView), aspectRatio, camera.nearPlane, camera.farPlane);

            return viewMatrix * projection;
        }

        return DirectX::XMMatrixIdentity();
    }
};