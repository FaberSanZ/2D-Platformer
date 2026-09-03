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
            registry.emplace<MeshComponent>(entity, ShapeType::RoundedRectangle);
            registry.emplace<TransformComponent>(entity, DirectX::XMFLOAT3{ -1.5f, 0.0f, 0.0f });
        }

    }
    void OnUpdate(entt::registry& registry) override
    {
		auto view = registry.view<TransformComponent, MeshComponent>();
        for(auto [entity, transform, mesh] : view.each())
        {
            transform.rotation -= 0.01f;
            transform.position.x += 0.01f;
		}
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

