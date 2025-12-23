#pragma once
#include <wtypes.h>

class Object
{
	int m_max_hp = 100;
	int m_hp = 50;
	float m_speed = 200.f;
	float m_currentSpeed= 0.f;
public:
	float m_x = 100;
	float m_y = 100;
	float m_size = 40;

public:
	Object() {};
	Object(float x, float y);

	void Draw(HDC hdc, float offsetX, float offsetY);

	float GetX() const { return m_x; };
	float GetY() const { return m_y; };
	float GetSpeed() const { return m_speed; };
	float GetSize() const { return m_size; };
	float GetCurrentSpeed() { return m_currentSpeed; };

	void SetPos(float x, float y) { m_x = x, m_y = y; };
	void SetcurrentSpeed(float speed) { m_currentSpeed = speed; };
	bool CheckCollision(const Object& other) const;

private:
	void DrawHp(HDC hdc, float centerX, float centerY);
};

