#pragma once
#include <wtypes.h>
#include <random>

#define MAP_HEIGHT 1000
#define MAP_WIDTH 1000
#define BlockSize 40

const int WINDOW_WIDTH = 1024;
const int WINDOW_HEIGHT = 768;

enum TileType {
	TILE_GRAY,
	TILE_LIME_GREEN,
	TILE_LIGHT_BROWN
};

class World
{
	TileType _mapTiles[MAP_HEIGHT][MAP_WIDTH];

	float offsetX;
	float offsetY;
public:
	void Init();
	void Draw(HDC hdc);
	void End();

	void UpdateOffset(float objX, float objY);
	float GetOffsetX() { return offsetX; };
	float GetOffsetY() { return offsetY; };
};

