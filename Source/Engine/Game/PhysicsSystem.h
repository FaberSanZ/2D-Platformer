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
			if (body.type == BodyType::Dynamic)
			{
				const float accelerationX = body.linearAcceleration.x + m_gravity.x;
				const float accelerationY = body.linearAcceleration.y + m_gravity.y;
				const float accelerationZ = body.linearAcceleration.z + m_gravity.z;

				body.linearVelocity.x += accelerationX * deltaTime;
				body.linearVelocity.y += accelerationY * deltaTime;
				body.linearVelocity.z += accelerationZ * deltaTime;
			}

			if (body.type != BodyType::Static)
			{
				transform.position.x += body.linearVelocity.x * deltaTime;
				transform.position.y += body.linearVelocity.y * deltaTime;
				transform.position.z += body.linearVelocity.z * deltaTime;
			}
		}
	}
	
private:

	DirectX::XMFLOAT3 m_gravity = { 0.0f, -9.81f * 0.1f, 0.0f };

};