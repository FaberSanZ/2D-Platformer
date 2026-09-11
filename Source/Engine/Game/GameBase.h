#include <entt/entt.hpp>
#include <unordered_map>
#include "GameWindow.h"
#include "Components.h"
#include "RenderSystem.h"
#include "GameTime.h"
#include "PhysicsSystem.h"
#include "CameraSystem.h"
#include "AssetSystem.h"

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

		m_gameTime.Reset();
		m_physicsSystem.Initialize();
		OnInitialize(registry);

		// Main game loop
		while (m_window.IsRunning())
		{
			m_window.PumpMessages();
			m_gameTime.Update();
			OnUpdate(registry);
			m_physicsSystem.Update(registry, m_gameTime.GetDeltaTime());

			float aspectRatio = static_cast<float>(m_window.ClientWidth()) / static_cast<float>(m_window.ClientHeight());

			DirectX::XMMATRIX viewProjection = m_cameraSystem.GetViewProjection(registry, aspectRatio);
			m_renderSystem.Update(viewProjection);

			//m_renderSystem.Update();
			m_renderSystem.BeginFrame();
			Render();
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

	virtual void OnInitialize(entt::registry& registry) = 0;
	virtual void OnUpdate(entt::registry& registry) = 0;
	virtual void OnDestroy(entt::registry& registry) = 0;

private:
	GameWindow m_window;
	GameTime m_gameTime;

	RenderSystem m_renderSystem;
	PhysicsSystem m_physicsSystem;
	CameraSystem m_cameraSystem;
	AssetSystem m_assetSystem;

	entt::registry registry;

	void Render()
	{
		std::unordered_map<Model*, std::vector<DirectX::XMFLOAT4X4>> batches;

		auto view = registry.view<TransformComponent, ModelComponent>();

		for (auto [entity, transform, model] : view.each())
		{
			DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(transform.scale.x, transform.scale.y, transform.scale.z);
			DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&transform.rotation));
			DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(transform.position.x, transform.position.y, transform.position.z);
			DirectX::XMMATRIX world = DirectX::XMMatrixTranspose(scale * rotation * translation);

			DirectX::XMFLOAT4X4 matrix;
			DirectX::XMStoreFloat4x4(&matrix, world);

			batches[model.model].push_back(matrix);
		}

		for (auto& [model, transforms] : batches)
			m_renderSystem.DrawModel(*model, transforms.data(), static_cast<uint32_t>(transforms.size()));
	}

	void Update()
	{

	}



	void Update2()
	{

	}


};