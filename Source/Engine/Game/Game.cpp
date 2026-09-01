// Game.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "GameBase.h"

class Game final : public GameBase
{

protected:
    void OnInitialize() override
    {
        std::cout << "Game Initialized\n";
    }
    void OnUpdate() override
    {
        std::cout << "Game Updated\n";
    }
    void OnDestroy() override
    {
        std::cout << "Game Destroyed\n";
    }
};

int main()
{
    Game game;
    game.Run();
	return 0;
}

