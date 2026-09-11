// Game.cpp : This file contains the 'main' function. Program execution begins and ends there.

#include <iostream>
#include <algorithm>
#include "GameBase.h"

class Game final : public GameBase
{
protected:
    Model* modelmesh;
    void OnInitialize(entt::registry& registry) override
    {
        // Create primary camera.
        auto camera = registry.create();

        registry.emplace<TransformComponent>(camera, DirectX::XMFLOAT3{ 0.0f, 0.0f, -7.0f });
        registry.emplace<CameraComponent>(camera);
        registry.emplace<PrimaryCameraComponent>(camera);

        // Load model.
        modelmesh = Assets().LoadModel("../Assets/Models/Spider.glb");
        Animations().Play(modelmesh, "SpiderArmature|Spider_Walk");

        auto entity = registry.create();

        auto& transform = registry.emplace<TransformComponent>(entity);
        transform.scale = { 1.0f, 1.0f, 1.0f };
        transform.position = { 0.0f, 0.0f, 0.0f };
        transform.scale = { 1.0f, 1.0f, 1.0f };
        DirectX::XMStoreFloat4(&transform.rotation, DirectX::XMQuaternionRotationRollPitchYaw(0.0f, DirectX::XMConvertToRadians(90.0f), 0.0f));



        auto& model = registry.emplace<ModelComponent>(entity);
        model.model = modelmesh;
        model.color = { 1.0f, 1.0f, 1.0f, 1.0f };

    }

    void OnUpdate(entt::registry& registry, float deltaTime) override
    {

        if (GameInput::IsKeyDown(GameInput::KeyCode::T)) 
        {
            Animations().Play(modelmesh, "SpiderArmature|Spider_Death", false);

        }

        auto view = registry.view<TransformComponent, CameraComponent, PrimaryCameraComponent>();

        for (auto [entity, transform, camera] : view.each())
        {
            if (GameInput::IsMouseButtonDown(GameInput::MouseButton::Right))
            {
                GameInput::SetMouseMode(GameInput::MouseMode::Relative);

                m_yaw += static_cast<float>(GameInput::GetMouseDeltaX()) * 0.0025f;
                m_pitch += static_cast<float>(GameInput::GetMouseDeltaY()) * 0.0025f;

                m_pitch = std::clamp(m_pitch, -1.5f, 1.5f);

                DirectX::XMVECTOR rotation = DirectX::XMQuaternionRotationRollPitchYaw(m_pitch, m_yaw, 0.0f);
                DirectX::XMStoreFloat4(&transform.rotation, rotation);

                DirectX::XMVECTOR forward = DirectX::XMVector3Rotate(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotation);
                DirectX::XMVECTOR right = DirectX::XMVector3Rotate(DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), rotation);
                DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&transform.position);

                float speed = 5.0f * deltaTime;

                if (GameInput::IsKeyDown(GameInput::KeyCode::W))
                    position = DirectX::XMVectorAdd(position, DirectX::XMVectorScale(forward, speed));

                if (GameInput::IsKeyDown(GameInput::KeyCode::S))
                    position = DirectX::XMVectorSubtract(position, DirectX::XMVectorScale(forward, speed));

                if (GameInput::IsKeyDown(GameInput::KeyCode::D))
                    position = DirectX::XMVectorAdd(position, DirectX::XMVectorScale(right, speed));

                if (GameInput::IsKeyDown(GameInput::KeyCode::A))
                    position = DirectX::XMVectorSubtract(position, DirectX::XMVectorScale(right, speed));

                DirectX::XMStoreFloat3(&transform.position, position);
            }
            else
            {
                GameInput::SetMouseMode(GameInput::MouseMode::Absolute);
            }

            // Only one PrimaryCameraComponent should exist.
            break;
        }
    }

    void OnDestroy(entt::registry& registry) override
    {
        GameInput::SetMouseMode(GameInput::MouseMode::Absolute);
    }

private:

    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
};

int main()
{
    Game game;
    game.Run();

    return 0;
}