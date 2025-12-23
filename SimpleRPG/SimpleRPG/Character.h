#pragma once
#include "Object.h"
class Character : public Object
{
public:
	void Draw(HDC hdc, float centerX, float centerY);
};

