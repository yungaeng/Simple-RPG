#include "Character.h"

void Character::Draw(HDC hdc, float centerX, float centerY)
{
    // 화면상의 좌표 = 실제 좌표 + 오프셋
    // 중요: m_x, m_y 대신 계산된 screenX, screenY를 전달해야 합니다!
    centerX += m_x;
    centerY += m_y;

    Object::Draw(hdc, centerX, centerY);

    // 사람그리기
    float headRadius = m_size / 4;
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
