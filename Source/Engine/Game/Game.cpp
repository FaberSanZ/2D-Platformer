// Game.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "GameBase.h"

class Game final : public GameBase
{

protected:
    void OnInitialize(entt::registry& registry) override
    {

        auto camera = registry.create();
        registry.emplace<TransformComponent>(camera, DirectX::XMFLOAT3{ 0.0f, 0.0f, -10.0f });
        registry.emplace<CameraComponent>(camera);
        registry.emplace<PrimaryCameraComponent>(camera);



        Model* model = Assets().LoadModel("../Assets/Models/Untitled.glb");

        auto entity = registry.create();
        registry.emplace<TransformComponent>(entity);
        registry.emplace<ModelComponent>(entity, model);
        
        
        //auto& body = registry.emplace<RigidBodyComponent>(entity);
		//body.linearVelocity = { 0.1f, 0.0f, 0.0f };
		//body.linearAcceleration = { 0.1f, 0.0f, 0.0f };

        Model* sphere = Assets().LoadModel("../Assets/Models/test.glb");
        for (int i = 0; i < 5; ++i)
        {
            auto entity = registry.create();

            auto& transform = registry.emplace<TransformComponent>(entity);
            transform.position = { static_cast<float>(i * 2 - 4), 0.0f, 0.0f };

            registry.emplace<ModelComponent>(entity, sphere);
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

