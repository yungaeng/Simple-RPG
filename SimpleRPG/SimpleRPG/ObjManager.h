#pragma once
#include "Object.h"
#include <cmath>

class ObjManager
{
	Object obj;
public:
	void MoveObj(char dir);
	void Update(float deltaTime);
	void Draw(HDC hdc);
};

