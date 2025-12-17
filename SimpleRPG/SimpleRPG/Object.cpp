#include "Object.h"
#include "Object.h"

Object::Object(float x, float y, COLORREF c)
{
    m_x = x;
    m_y = y;
    m_color = c;

    m_isalive = true;
    m_size = 10;
    m_speed = 300.f;
}

void Object::Draw(HDC hdc)
{
    if (m_isalive) {
        drawperson(hdc, m_x, m_y);
        drawhpbar(hdc, m_x, m_y);
    }
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

void Object::drawperson(HDC hdc, int centerX, int centerY) {
    int headRadius = m_size / 4;
    Ellipse(hdc, centerX - headRadius, centerY - m_size / 2, centerX + headRadius, centerY - m_size / 2 + headRadius * 2);
    MoveToEx(hdc, centerX, centerY - m_size / 2 + headRadius * 2, NULL);
    LineTo(hdc, centerX, centerY + m_size / 4);
    MoveToEx(hdc, centerX, centerY - m_size / 2 + headRadius * 2 + m_size / 8, NULL);
    LineTo(hdc, centerX - m_size / 4, centerY + m_size / 8);
    MoveToEx(hdc, centerX, centerY - m_size / 2 + headRadius * 2 + m_size / 8, NULL);
    LineTo(hdc, centerX + m_size / 4, centerY + m_size / 8);
    MoveToEx(hdc, centerX, centerY + m_size / 4, NULL);
    LineTo(hdc, centerX - m_size / 6, centerY + m_size / 2);
    MoveToEx(hdc, centerX, centerY + m_size / 4, NULL);
    LineTo(hdc, centerX + m_size / 6, centerY + m_size / 2);
}

void Object::drawslime(HDC hdc, int centerX, int centerY)
{
    int slimeWidth = m_size * 3 / 4;
    int slimeHeight = m_size * 2 / 3;
    Ellipse(hdc, centerX - slimeWidth / 2, centerY - slimeHeight / 2,
        centerX + slimeWidth / 2, centerY + slimeHeight / 2);
    int eyeRadius = m_size / 12;
    int eyeOffsetY = m_size / 8;
    int eyeOffsetX = m_size / 6;
    Ellipse(hdc, centerX - eyeOffsetX - eyeRadius, centerY - eyeOffsetY - eyeRadius,
        centerX - eyeOffsetX + eyeRadius, centerY - eyeOffsetY + eyeRadius);
    Ellipse(hdc, centerX + eyeOffsetX - eyeRadius, centerY - eyeOffsetY - eyeRadius,
        centerX + eyeOffsetX + eyeRadius, centerY - eyeOffsetY + eyeRadius);
}

void Object::drawhpbar(HDC hdc, int centerX, int centerY)
{
    if (m_max_hp > 0)
    {
        int barWidth = m_size;
        int barHeight = 5;

        int barTop = centerY - m_size / 2 - barHeight - 5;
        int barLeft = centerX - barWidth / 2;
        int barRight = centerX + barWidth / 2;
        int barBottom = barTop + barHeight;

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