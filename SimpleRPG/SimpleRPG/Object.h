#pragma once
#include <wtypes.h>

class Object
{
	float m_x = 100;
	float m_y = 100;
	
	int m_max_hp = 100;
	int m_hp = 50;
	float m_size = 40;
	float m_speed = 200.f;

	COLORREF m_color;
	bool m_isalive = true;
public:
	Object() {};
	Object(float x, float y, COLORREF c);

	void Draw(HDC hdc, float offsetX, float offsetY);

	float GetX() const { return m_x; };
	float GetY() const { return m_y; };
	float GetSpeed() const { return m_speed; };
	float GetSize() const { return m_size; };
	COLORREF GetColor() { return m_color; };

	void SetAlive(bool alive) { m_isalive = alive; }
	void SetPos(float x, float y) { m_x = x, m_y = y; };
	void SetColor(COLORREF color) { m_color = color; };
	void SetDie() { m_isalive = false; };

	bool CheckCollision(const Object& other) const;

	void drawperson(HDC hdc, int centerX, int centerY);
	void drawslime(HDC hdc, int centerX, int centerY);
	void drawhpbar(HDC hdc, int centerX, int centerY);
};

