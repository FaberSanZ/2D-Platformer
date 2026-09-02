// Game.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "GameBase.h"

class Game final : public GameBase
{

protected:
    void OnInitialize(entt::registry& registry) override
    {
        {
            auto entity = registry.create();
            registry.emplace<MeshComponent>(entity, ShapeType::Sprite);
            registry.emplace<TransformComponent>(entity, DirectX::XMFLOAT3{ -2.0f, 1.0f, 0.0f });
        }

        {
            auto entity = registry.create();
            registry.emplace<MeshComponent>(entity, ShapeType::Rectangle);
            registry.emplace<TransformComponent>(entity, DirectX::XMFLOAT3{ -1.0f, 1.0f, 0.0f });
        }

        {
            auto entity = registry.create();
            registry.emplace<MeshComponent>(entity, ShapeType::Circle);
            registry.emplace<TransformComponent>(entity, DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f });
        }

        {
            auto entity = registry.create();
            registry.emplace<MeshComponent>(entity, ShapeType::Capsule);
            registry.emplace<TransformComponent>(entity, DirectX::XMFLOAT3{ 1.0f, 1.0f, 0.0f });
        }

        {
            auto entity = registry.create();
            registry.emplace<MeshComponent>(entity, ShapeType::CapsuleBetween);
            registry.emplace<TransformComponent>(entity, DirectX::XMFLOAT3{ 2.0f, 1.0f, 0.0f });
        }

        {
            auto entity = registry.create();
            registry.emplace<MeshComponent>(entity, ShapeType::RoundedRectangle);
            registry.emplace<TransformComponent>(entity, DirectX::XMFLOAT3{ -0.75f, -0.5f, 0.0f });
        }

        {
            auto entity = registry.create();
            registry.emplace<MeshComponent>(entity, ShapeType::ConvexPolygon);
            registry.emplace<TransformComponent>(entity, DirectX::XMFLOAT3{ 0.75f, -0.5f, 0.0f });
        }
    }
    void OnUpdate(entt::registry& registry) override
    {

    }
    void OnDestroy(entt::registry& registry) override
    {
    }
};

int main()
{
    Game game;
    game.Run();
	return 0;
}

