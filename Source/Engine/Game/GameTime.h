#pragma once
#include <chrono>
#include <algorithm>

class GameTime
{
public:
	using Clock = std::chrono::high_resolution_clock;

	void Reset()
	{
		m_previousTime = Clock::now();
		m_deltaTime = 0.0f;
	}

	void Update()
	{
		auto currentTime = Clock::now();
		const float elapsedTime = std::chrono::duration<float>(currentTime - m_previousTime).count();

		m_previousTime = currentTime;
		m_deltaTime = std::min(elapsedTime, 0.25f); // Clamp delta time to avoid large jumps
	}

	float GetDeltaTime() const
	{
		return m_deltaTime;
	}


private:
	Clock::time_point m_previousTime = Clock::now();
	float m_deltaTime = 0.0f;
};
