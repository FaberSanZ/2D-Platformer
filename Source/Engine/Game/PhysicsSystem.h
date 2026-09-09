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
		auto view = registry.view<RigidBodyComponent, TransformComponent>();
		for (auto [entity, body, transform] : view.each())
		{
			body.position.x += body.linearVelocity.x * deltaTime;
			body.position.y += body.linearVelocity.y * deltaTime;

			transform.position.x += 0.6f * deltaTime;
			transform.position.x += 0.6f * deltaTime;
		}
	}


};