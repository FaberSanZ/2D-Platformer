#pragma once

class PhysicsSystem
{
public:
	void Initialize()
	{
		// Initialize physics system
	}

	void Update(entt::registry& registry, float deltaTime)
	{
		// test code to update the transform of entities with TransformComponent and MeshComponent
		auto view = registry.view<TransformComponent, MeshComponent>();
		for (auto [entity, transform, mesh] : view.each())
		{
			transform.rotation -= 0.01f;
			transform.position.x += 0.01f;
		}
	}


};