#include "Object.h"

Object::Object(float x, float y, COLORREF c)
{
    m_x = x;
    m_y = y;
    m_color = c;

    m_isalive = true;
    m_size = 10;
    m_speed = 20;
}

void Object::Draw(HDC hdc)
{
    if (m_isalive) {
        RECT ObjRect = { m_x - m_size, m_y - m_size, m_x + m_size, m_y + m_size };
        HBRUSH ObjBrush = CreateSolidBrush(m_color);
        FillRect(hdc, &ObjRect, ObjBrush);
        DeleteObject(ObjBrush);
    }
}

bool Object::CheckCollision(const Object& other) const
{
    // 현재 객체의 경계
    int leftA = m_x - m_size /2;
    int rightA = m_x + m_size /2;
    int topA = m_y - m_size /2;
    int bottomA = m_y + m_size /2;

    // 다른 객체의 경계
    int leftB = other.m_x - other.m_size/2;
    int rightB = other.m_x + other.m_size/2;
    int topB = other.m_y - other.m_size/2;
    int bottomB = other.m_y + other.m_size/2;

    // AABB (Axis-Aligned Bounding Box) 충돌 확인:
    if (bottomA <= topB || topA >= bottomB || rightA <= leftB || leftA >= rightB)
        return false;
    return true;
}
