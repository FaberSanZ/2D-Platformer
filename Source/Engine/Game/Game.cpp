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

			RigidBodyComponent body;
			body.position = { 0.0f, 3.0f };
			body.linearVelocity = { 0.5f, 0.0f };
            body.linearAcceleration = { 0.0f, 0.0f };
			body.type = BodyType::Dynamic;

            registry.emplace<RigidBodyComponent>(entity, body);
            registry.emplace<MeshComponent>(entity, ShapeType::Circle);
            registry.emplace<TransformComponent>(entity, DirectX::XMFLOAT3{ -1.8f, 0.0f, 0.0f });
        }


        {
            auto entity = registry.create();

            RigidBodyComponent body;
            body.position = { -1.0f, 3.0f };
            body.linearVelocity = { 0.5f, 0.0f };
            body.linearAcceleration = { 0.0f, 0.0f };
            body.type = BodyType::Kinematic;

            registry.emplace<RigidBodyComponent>(entity, body);
            registry.emplace<MeshComponent>(entity, ShapeType::Circle);
            registry.emplace<TransformComponent>(entity, DirectX::XMFLOAT3{ -1.8f, 0.0f, 0.0f });
        }


        {
            auto entity = registry.create();

            RigidBodyComponent body;
            body.position = { -2.0f, 3.0f };
            body.linearVelocity = { 0.5f, 0.0f };
            body.linearAcceleration = { 0.0f, 0.0f };
            body.type = BodyType::Static;

            registry.emplace<RigidBodyComponent>(entity, body);
            registry.emplace<MeshComponent>(entity, ShapeType::Circle);
            registry.emplace<TransformComponent>(entity, DirectX::XMFLOAT3{ -1.8f, 0.0f, 0.0f });
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

