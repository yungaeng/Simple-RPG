#include "Slime.h"
void Slime::Draw(HDC hdc, float centerX, float centerY)
{
    // 화면상의 좌표 = 실제 좌표 + 오프셋
    // 중요: m_x, m_y 대신 계산된 screenX, screenY를 전달해야 합니다!
    centerX += m_x;
    centerY += m_y;

    // hp바 그리기
    Object::Draw(hdc, centerX, centerY);

    // 슬라임 그리기
    float slimeWidth = m_size * 3 / 4;
    float slimeHeight = m_size * 2 / 3;
    Ellipse(hdc, centerX - slimeWidth / 2, centerY - slimeHeight / 2,
        centerX + slimeWidth / 2, centerY + slimeHeight / 2);
    float eyeRadius = m_size / 12;
    float eyeOffsetY = m_size / 8;
    float eyeOffsetX = m_size / 6;
    Ellipse(hdc, centerX - eyeOffsetX - eyeRadius, centerY - eyeOffsetY - eyeRadius,
        centerX - eyeOffsetX + eyeRadius, centerY - eyeOffsetY + eyeRadius);
    Ellipse(hdc, centerX + eyeOffsetX - eyeRadius, centerY - eyeOffsetY - eyeRadius,
        centerX + eyeOffsetX + eyeRadius, centerY - eyeOffsetY + eyeRadius);
}