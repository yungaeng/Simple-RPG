#pragma once
#include "Object.h"
#include <unordered_map>
#include <vector>
#include <string>
#include "../Server/game_header.h"
constexpr unsigned short BlockSize = 20 * 2;
constexpr int SCREEN_WIDTH = 660;
constexpr int SCREEN_HEIGHT = 600;


enum TileType {
    TILE_GRAY,
    TILE_LIME_GREEN,
    TILE_LIGHT_BROWN
};

class Map
{
    long long myPlayerId = -1; // 자신의 ID를 저장할 변수
    int offsetX;
    int offsetY;
    std::unordered_map<long long , Object> objects;
    TileType _mapTiles[MAP_HEIGHT][MAP_WIDTH];

    std::vector<std::wstring> chatMessages; // Stores recent chat messages
    const int MAX_CHAT_MESSAGES = 5; // Or however many you want to show
    const int CHAT_START_Y = 50; // Starting Y coordinate for chat
    const int CHAT_LINE_HEIGHT = 20; // Height of each chat line
public:
    Map() {
        InitializeMapTiles();
    };
    ~Map() {};
    
    void Setoffset(int x, int y);
    void Draw(HDC hdc) {
        DrawMapAndObj(hdc);
        DrawUI(hdc);
    };

    void createobj(long long  id, short x, short y, char type, int hp, int maxhp, int exp, int level);
    void addobj(long long  id, short x, short y, char type);
    void deleteobj(long long id);
    void stat_change(long long id, int hp, int maxhp, int exp, int level);
    void moveobj(long long id, short x, short y);
    void chat(long long id, const char chatMsg[100]);
    // Helper to convert char* to wstring
    std::wstring ConvertCharToWString(const char* narrowStr);
private:
    void DrawMapAndObj(HDC hdc);
    void DrawUI(HDC hdc);
    void InitializeMapTiles();
};

