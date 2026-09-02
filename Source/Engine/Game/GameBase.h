#include "GameWindow.h"
#include "RenderSystem.h"

using namespace Vultaik;

class GameBase
{
public:
	GameBase() = default;


	void Run()
	{
		m_window.Initialize();
		m_window.SetTitle(L"My game engine");
		m_window.SetWindowSize(800, 600);

		OnInitialize();

		m_renderSystem.Initialize(m_window.Handle(), m_window.ClientWidth(), m_window.ClientHeight());


		// Main game loop
		while (m_window.IsRunning())
		{
			m_window.PumpMessages();
			OnUpdate();

			m_renderSystem.Update();
			m_renderSystem.BeginFrame();
			m_renderSystem.Render();
			m_renderSystem.EndFrame();
		}

		OnDestroy();
	}

protected:

	virtual void OnInitialize() = 0;
	virtual void OnUpdate() = 0;
	virtual void OnDestroy() = 0;

private:

	GameWindow m_window;
	RenderSystem m_renderSystem;
};