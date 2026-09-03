#include "GameWindow.h"
#include "Components.h"
#include "RenderSystem.h"
#include <entt/entt.hpp>

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

		OnInitialize(registry);



		const wchar_t* m_texturePath = L"../Assets/Textures/deco_check_bw_01_ff8800_512.png";


		m_renderSystem.Initialize(m_window.Handle(), m_window.ClientWidth(), m_window.ClientHeight());
		//auto capsule = Shapes2D::CreateCapsule(0.35f, 1.20f);

		auto sprite = Shapes2D::CreateSpriteQuad(0.8f, 0.8f);
		m_spriteMesh = m_renderSystem.CreateMesh(sprite.Vertices.data(), static_cast<uint32_t>(sprite.Vertices.size() * sizeof(Shapes2D::Vertex)), sprite.Indices.data(), static_cast<uint32_t>(sprite.Indices.size() * sizeof(uint32_t)), 64 * 64, m_texturePath);

		auto rectangle = Shapes2D::CreateRectangle(1.0f, 0.6f);
		m_rectangleMesh = m_renderSystem.CreateMesh(rectangle.Vertices.data(), static_cast<uint32_t>(rectangle.Vertices.size() * sizeof(Shapes2D::Vertex)), rectangle.Indices.data(), static_cast<uint32_t>(rectangle.Indices.size() * sizeof(uint32_t)), 64 * 64, m_texturePath);

		auto circle = Shapes2D::CreateCircle(0.45f, 32);
		m_circleMesh = m_renderSystem.CreateMesh(circle.Vertices.data(), static_cast<uint32_t>(circle.Vertices.size() * sizeof(Shapes2D::Vertex)), circle.Indices.data(), static_cast<uint32_t>(circle.Indices.size() * sizeof(uint32_t)), 64 * 64, m_texturePath);

		auto capsule = Shapes2D::CreateCapsule(0.25f, 0.65f, Shapes2D::CapsuleAxis::Vertical, 12);
		m_capsuleMesh = m_renderSystem.CreateMesh(capsule.Vertices.data(), static_cast<uint32_t>(capsule.Vertices.size() * sizeof(Shapes2D::Vertex)), capsule.Indices.data(), static_cast<uint32_t>(capsule.Indices.size() * sizeof(uint32_t)), 64 * 64, m_texturePath);

		auto capsuleBetween = Shapes2D::CreateCapsuleBetween({ -0.35f, 0.0f }, { 0.35f, 0.0f }, 0.20f, 10);
		m_capsuleBetweenMesh = m_renderSystem.CreateMesh(capsuleBetween.Vertices.data(), static_cast<uint32_t>(capsuleBetween.Vertices.size() * sizeof(Shapes2D::Vertex)), capsuleBetween.Indices.data(), static_cast<uint32_t>(capsuleBetween.Indices.size() * sizeof(uint32_t)), 64 * 64, m_texturePath);

		auto roundedRectangle = Shapes2D::CreateRoundedRectangle(1.0f, 0.7f, 0.16f, 5);
		m_roundedRectangleMesh = m_renderSystem.CreateMesh(roundedRectangle.Vertices.data(), static_cast<uint32_t>(roundedRectangle.Vertices.size() * sizeof(Shapes2D::Vertex)), roundedRectangle.Indices.data(), static_cast<uint32_t>(roundedRectangle.Indices.size() * sizeof(uint32_t)), 64 * 64, m_texturePath);

		const std::array<DirectX::XMFLOAT2, 5> polygonPoints =
		{
			DirectX::XMFLOAT2{  0.0f,  0.50f },
			DirectX::XMFLOAT2{ -0.48f,  0.15f },
			DirectX::XMFLOAT2{ -0.30f, -0.45f },
			DirectX::XMFLOAT2{  0.30f, -0.45f },
			DirectX::XMFLOAT2{  0.48f,  0.15f }
		};

		auto polygon = Shapes2D::CreateConvexPolygon(polygonPoints);
		m_polygonMesh = m_renderSystem.CreateMesh(polygon.Vertices.data(), static_cast<uint32_t>(polygon.Vertices.size() * sizeof(Shapes2D::Vertex)), polygon.Indices.data(), static_cast<uint32_t>(polygon.Indices.size() * sizeof(uint32_t)), 64 * 64, m_texturePath);

		// Main game loop
		while (m_window.IsRunning())
		{
			m_window.PumpMessages();
			OnUpdate(registry);
			Update();

			m_renderSystem.Update();
			m_renderSystem.BeginFrame();
			Render();
			m_renderSystem.EndFrame();
		}

		OnDestroy(registry);

		CoUninitialize();
	}

protected:

	virtual void OnInitialize(entt::registry& registry) = 0;
	virtual void OnUpdate(entt::registry& registry) = 0;
	virtual void OnDestroy(entt::registry& registry) = 0;

private:

	GameWindow m_window;
	RenderSystem m_renderSystem;

	Mesh2D m_spriteMesh;
	Mesh2D m_rectangleMesh;
	Mesh2D m_circleMesh;
	Mesh2D m_capsuleMesh;
	Mesh2D m_capsuleBetweenMesh;
	Mesh2D m_roundedRectangleMesh;
	Mesh2D m_polygonMesh;

	entt::registry registry;

	void Render()
	{
		m_renderSystem.DrawMesh(m_spriteMesh);
		m_renderSystem.DrawMesh(m_rectangleMesh);
		m_renderSystem.DrawMesh(m_circleMesh);
		m_renderSystem.DrawMesh(m_capsuleMesh);
		m_renderSystem.DrawMesh(m_capsuleBetweenMesh);
		m_renderSystem.DrawMesh(m_roundedRectangleMesh);
		m_renderSystem.DrawMesh(m_polygonMesh);
	}

	void Update()
	{
		std::vector<DirectX::XMMATRIX> spriteData;
		std::vector<DirectX::XMMATRIX> rectangleData;
		std::vector<DirectX::XMMATRIX> circleData;
		std::vector<DirectX::XMMATRIX> capsuleData;
		std::vector<DirectX::XMMATRIX> capsuleBetweenData;
		std::vector<DirectX::XMMATRIX> roundedRectangleData;
		std::vector<DirectX::XMMATRIX> polygonData;

		auto view_mesh = registry.view<TransformComponent, MeshComponent>();

		for (auto [entity, transform, mesh] : view_mesh.each())
		{
			DirectX::XMMATRIX pos = DirectX::XMMatrixTranslation(transform.position.x, transform.position.y, transform.position.z);
			DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationZ(transform.rotation);
			DirectX::XMMATRIX model = DirectX::XMMatrixTranspose(rotation * pos);

			switch (mesh.shapeType)
			{
			case ShapeType::Sprite:
				spriteData.push_back(model);
				break;

			case ShapeType::Rectangle:
				rectangleData.push_back(model);
				break;

			case ShapeType::Circle:
				circleData.push_back(model);
				break;

			case ShapeType::Capsule:
				capsuleData.push_back(model);
				break;

			case ShapeType::CapsuleBetween:
				capsuleBetweenData.push_back(model);
				break;

			case ShapeType::RoundedRectangle:
				roundedRectangleData.push_back(model);
				break;

			case ShapeType::ConvexPolygon:
				polygonData.push_back(model);
				break;
			}
		}

		if (!spriteData.empty())
			m_renderSystem.UpdateGpuData(m_spriteMesh.instances, spriteData.data(), static_cast<uint32_t>(spriteData.size()));

		if (!rectangleData.empty())
			m_renderSystem.UpdateGpuData(m_rectangleMesh.instances, rectangleData.data(), static_cast<uint32_t>(rectangleData.size()));

		if (!circleData.empty())
			m_renderSystem.UpdateGpuData(m_circleMesh.instances, circleData.data(), static_cast<uint32_t>(circleData.size()));

		if (!capsuleData.empty())
			m_renderSystem.UpdateGpuData(m_capsuleMesh.instances, capsuleData.data(), static_cast<uint32_t>(capsuleData.size()));

		if (!capsuleBetweenData.empty())
			m_renderSystem.UpdateGpuData(m_capsuleBetweenMesh.instances, capsuleBetweenData.data(), static_cast<uint32_t>(capsuleBetweenData.size()));

		if (!roundedRectangleData.empty())
			m_renderSystem.UpdateGpuData(m_roundedRectangleMesh.instances, roundedRectangleData.data(), static_cast<uint32_t>(roundedRectangleData.size()));

		if (!polygonData.empty())
			m_renderSystem.UpdateGpuData(m_polygonMesh.instances, polygonData.data(), static_cast<uint32_t>(polygonData.size()));
	}



	void Update2()
	{

	}

	void UpdateMeshTransform(Mesh2D& mesh, float x, float y)
	{
		DirectX::XMMATRIX model = DirectX::XMMatrixTranslation(x, y, 0.0f);
		model = DirectX::XMMatrixTranspose(model);
		m_renderSystem.UpdateGpuData(mesh.instances, &model, 1);
	}
};