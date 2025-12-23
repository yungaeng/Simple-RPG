#pragma once
#include "Character.h"
#include <cmath>

class ObjManager
{
	Character character;
public:
	void Update(float deltaTime);
	void Draw(HDC hdc, float offsetX, float offsetY);

	void GetOffset(float* x, float* y) { *x = character.GetX(); *y = character.GetY();}
};

