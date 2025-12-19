#pragma once
#include "Object.h"
#include <cmath>

class ObjManager
{
	Object obj;
public:
	void Update(float deltaTime);
	void Draw(HDC hdc, float offsetX, float offsetY);

	void GetOffset(float* x, float* y) { *x = obj.GetX(); *y = obj.GetY();}
};

