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
			// Only dynamic bodies respond to acceleration and gravity.
			if (body.type == BodyType::Dynamic)
			{
				const float accelerationX = body.linearAcceleration.x + m_gravity.x;
				const float accelerationY = body.linearAcceleration.y + m_gravity.y;

				// Update velocity from acceleration.
				body.linearVelocity.x += accelerationX * deltaTime;
				body.linearVelocity.y += accelerationY * deltaTime;
			}


			// Dynamic and kinematic bodies move according to their velocity.
			if(body.type != BodyType::Static)
			{
				body.position.x += body.linearVelocity.x * deltaTime;
				body.position.y += body.linearVelocity.y * deltaTime;
			}




			// Synchronize the render transform.
			transform.position.x = body.position.x;
			transform.position.y = body.position.y;
		}
	}
	
private:

	DirectX::XMFLOAT2 m_gravity = { 0.0f, -9.81f * 0.1f };


};