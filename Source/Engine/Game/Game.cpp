// Game.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "GameBase.h"

class Game final : public GameBase
{

protected:
    void OnInitialize() override
    {
    }
    void OnUpdate() override
    {
    }
    void OnDestroy() override
    {
    }
};

int main()
{
    Game game;
    game.Run();
	return 0;
}

