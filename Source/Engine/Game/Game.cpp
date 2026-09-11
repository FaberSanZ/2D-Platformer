// Game.cpp : This file contains the 'main' function. Program execution begins and ends there.

#include <iostream>
#include <algorithm>
#include "GameBase.h"

class Game final : public GameBase
{
protected:
    Model* modelmesh = nullptr;

    void OnInitialize(entt::registry& registry) override
    {
        // Create primary camera.
        auto camera = registry.create();

        auto& cameraTransform = registry.emplace<TransformComponent>(camera);
        cameraTransform.position = { 0.0f, 2.0f, -20.0f };
        cameraTransform.scale = { 1.0f, 1.0f, 1.0f };
        DirectX::XMStoreFloat4(&cameraTransform.rotation, DirectX::XMQuaternionRotationRollPitchYaw(-0.12f, 0.0f, 0.0f));

        registry.emplace<CameraComponent>(camera);
        registry.emplace<PrimaryCameraComponent>(camera);

        m_yaw = 0.0f;
        m_pitch = -0.12f;

        // Load model.
        modelmesh = Assets().LoadModel("../Assets/Models/Spider.glb");

        CreateSpider(registry, { -12.0f, 0.0f, 0.0f }, { 1.0f, 0.2f, 0.2f, 1.0f }, "SpiderArmature|Spider_Idle", true);
        CreateSpider(registry, { -6.0f, 0.0f, 0.0f }, { 0.2f, 1.0f, 0.2f, 1.0f }, "SpiderArmature|Spider_Walk", true);
        CreateSpider(registry, { 0.0f, 0.0f, 0.0f }, { 0.2f, 0.5f, 1.0f, 1.0f }, "SpiderArmature|Spider_Attack", true);
        CreateSpider(registry, { 6.0f, 0.0f, 0.0f }, { 1.0f, 0.8f, 0.2f, 1.0f }, "SpiderArmature|Spider_Jump", true);
        CreateSpider(registry, { 12.0f, 0.0f, 0.0f }, { 0.8f, 0.2f, 1.0f, 1.0f }, "SpiderArmature|Spider_Death", false);
    }

    void OnUpdate(entt::registry& registry, float deltaTime) override
    {
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

                float speed = 8.0f * deltaTime;

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

            break;
        }
    }

    void OnDestroy(entt::registry& registry) override
    {
        GameInput::SetMouseMode(GameInput::MouseMode::Absolute);
    }

private:
    entt::entity CreateSpider(entt::registry& registry, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT4& color, const char* animationName, bool loop)
    {
        entt::entity entity = registry.create();

        auto& transform = registry.emplace<TransformComponent>(entity);
        transform.position = position;
        transform.scale = { 1.0f, 1.0f, 1.0f };
        DirectX::XMStoreFloat4(&transform.rotation, DirectX::XMQuaternionRotationRollPitchYaw(0.0f, DirectX::XMConvertToRadians(90.0f), 0.0f));

        auto& model = registry.emplace<ModelComponent>(entity);
        model.model = modelmesh;
        model.color = color;

        Animations().Play(registry, entity, animationName, loop);

        return entity;
    }

    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
};

int main()
{
    Game game;
    game.Run();

    return 0;
}