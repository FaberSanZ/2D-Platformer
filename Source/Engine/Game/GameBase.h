#include "GameWindow.h"

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

		while (m_window.IsRunning())
		{
			m_window.PumpMessages();
			OnUpdate();
		}

		OnDestroy();
	}

protected:

	virtual void OnInitialize() = 0;
	virtual void OnUpdate() = 0;
	virtual void OnDestroy() = 0;

private:

	GameWindow m_window;
};