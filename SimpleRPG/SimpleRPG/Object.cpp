#include "Object.h"
#include "Object.h"

Object::Object(float x, float y)
{
    m_x = x;
    m_y = y;

    m_size = 10;
    m_speed = 200.f;
}

void Object::Draw(HDC hdc, float offsetX, float offsetY)
{
    DrawHp(hdc, offsetX, offsetY);
}

bool Object::CheckCollision(const Object& other) const
{
    // 현재 객체의 경계
    int leftA = m_x - m_size / 2;
    int rightA = m_x + m_size / 2;
    int topA = m_y - m_size / 2;
    int bottomA = m_y + m_size / 2;

    // 다른 객체의 경계
    int leftB = other.m_x - other.m_size / 2;
    int rightB = other.m_x + other.m_size / 2;
    int topB = other.m_y - other.m_size / 2;
    int bottomB = other.m_y + other.m_size / 2;

    // AABB (Axis-Aligned Bounding Box) 충돌 확인:
    if (bottomA <= topB || topA >= bottomB || rightA <= leftB || leftA >= rightB)
        return false;
    return true;
}

void Object::DrawHp(HDC hdc, float centerX, float centerY)
{
    if (m_max_hp > 0)
    {
        float barWidth = m_size;
        float barHeight = 5;

        float barTop = centerY - m_size / 2 - barHeight - 5;
        float barLeft = centerX - barWidth / 2;
        float barRight = centerX + barWidth / 2;
        float barBottom = barTop + barHeight;

        RECT bgRect = { barLeft, barTop, barRight, barBottom };
        HBRUSH bgBrush = CreateSolidBrush(RGB(50, 50, 50));
        FillRect(hdc, &bgRect, bgBrush);
        DeleteObject(bgBrush);

        int currentHpWidth = (int)((double)m_hp / m_max_hp * barWidth);

        RECT hpRect = { barLeft, barTop, barLeft + currentHpWidth, barBottom };
        HBRUSH hpBrush = CreateSolidBrush(RGB(0, 200, 0));
        FillRect(hdc, &hpRect, hpBrush);
        DeleteObject(hpBrush);
    }
}