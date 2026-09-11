#include <entt/entt.hpp>
#include <unordered_map>
#include <vector>
#include "GameWindow.h"
#include "Components.h"
#include "RenderSystem.h"
#include "GameTime.h"
#include "PhysicsSystem.h"
#include "CameraSystem.h"
#include "AssetSystem.h"
#include "AnimationSystem.h"
#include "EditorSystem.h"

using namespace Vultaik;

class GameBase
{
public:
	GameBase() = default;


	void Run()
	{
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);

		m_window.Initialize();
		m_window.SetTitle(L"My game engine");
		m_window.SetWindowSize(1640, 820);


		m_renderSystem.Initialize(m_window.Handle(), m_window.ClientWidth(), m_window.ClientHeight());
		m_assetSystem.Initialize(&m_renderSystem);
		m_editorSystem.Initialize(m_window.Handle(), m_renderSystem.Device(), m_renderSystem.Context());


		m_gameTime.Reset();
		m_physicsSystem.Initialize();
		OnInitialize(registry);

		// Main game loop
		while (m_window.IsRunning())
		{
			m_window.PumpMessages();
			m_gameTime.Update();

			m_editorSystem.BeginFrame();

			OnUpdate(registry, m_gameTime.GetDeltaTime());
			m_animationSystem.Update(registry, m_gameTime.GetDeltaTime());
			m_physicsSystem.Update(registry, m_gameTime.GetDeltaTime());

			float aspectRatio = static_cast<float>(m_window.ClientWidth()) / static_cast<float>(m_window.ClientHeight());

			DirectX::XMMATRIX viewProjection = m_cameraSystem.GetViewProjection(registry, aspectRatio);
			m_renderSystem.Update(viewProjection);

			m_renderSystem.BeginFrame();

			Render();

			m_editorSystem.Draw(registry, m_animationSystem);
			m_editorSystem.EndFrame();

			m_renderSystem.EndFrame();
		}

		OnDestroy(registry);

		CoUninitialize();
	}

protected:
	AssetSystem& Assets() { return m_assetSystem; }
	RenderSystem& Renderer() { return m_renderSystem; }
	PhysicsSystem& Physics() { return m_physicsSystem; }
	CameraSystem& Camera() { return m_cameraSystem; }
	AnimationSystem& Animations() { return m_animationSystem; }

	virtual void OnInitialize(entt::registry& registry) = 0;
	virtual void OnUpdate(entt::registry& registry, float deltaTime) = 0;
	virtual void OnDestroy(entt::registry& registry) = 0;

private:
	GameWindow m_window;
	GameTime m_gameTime;

	RenderSystem m_renderSystem;
	PhysicsSystem m_physicsSystem;
	CameraSystem m_cameraSystem;
	AssetSystem m_assetSystem;
	AnimationSystem m_animationSystem;
	EditorSystem m_editorSystem;

	entt::registry registry;

	void Render()
	{
		std::unordered_map<Model*, std::vector<InstanceData>> batches;

		auto view = registry.view<TransformComponent, ModelComponent>();

		for (auto [entity, transform, model] : view.each())
		{
			if (!model.model)
				continue;

			DirectX::XMVECTOR rotation = DirectX::XMLoadFloat4(&transform.rotation);

			InstanceData instance{};

			instance.world = DirectX::XMMatrixTranspose(
				DirectX::XMMatrixScaling(transform.scale.x, transform.scale.y, transform.scale.z) *
				DirectX::XMMatrixRotationQuaternion(rotation) *
				DirectX::XMMatrixTranslation(transform.position.x, transform.position.y, transform.position.z));

			instance.baseColor = model.color;

			AnimationComponent* animation = registry.try_get<AnimationComponent>(entity);

			if (animation && !animation->pose.empty())
			{
				m_renderSystem.DrawModel(*model.model, &instance, 1, &animation->pose);
				continue;
			}

			batches[model.model].push_back(instance);
		}

		for (auto& [model, instances] : batches)
			m_renderSystem.DrawModel(*model, instances.data(), static_cast<uint32_t>(instances.size()));
	}

	void Update()
	{

	}



	void Update2()
	{

	}


};