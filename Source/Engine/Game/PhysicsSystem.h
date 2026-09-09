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
			// Update velocity from acceleration.
			body.linearVelocity.x += body.linearAcceleration.x * deltaTime;
			body.linearVelocity.y += body.linearAcceleration.y * deltaTime;

			body.position.x += body.linearVelocity.x * deltaTime;
			body.position.y += body.linearVelocity.y * deltaTime;

			// Synchronize the render transform.
			transform.position.x = body.position.x;
			transform.position.y = body.position.y;
		}
	}


};