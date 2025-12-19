#pragma once
#include "string"
#include "World.h"
#include <chrono>
#include "ObjManager.h"
#include <wtypes.h>

struct CharacterInfo {
    std::wstring name;
    int jobType;
};

class Game
{
    CharacterInfo player;
    World world;
    ObjManager obj;
    std::chrono::time_point<std::chrono::steady_clock> m_timer;
   
public:
    void Init();
    void Draw(HDC hdc);
	void Input(WPARAM wParam, LPARAM lParam);
    void Update();
	void End();

    void SetCharacterInfo(const wchar_t* name, int job) { player.name = name, player.jobType = job; }
    CharacterInfo GetCharacterInfo() { return player; };
private:
    double GetElapsedTime();
};

