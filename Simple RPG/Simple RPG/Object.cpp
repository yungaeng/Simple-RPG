#include "Object.h"
#include <sstream>
#include "../Server/game_header.h"

Object::Object(long long id, short x, short y, char type, int hp, int maxhp, int exp, int level)
{
    ZeroMemory(name, sizeof(name));
    _id = id; _x = x; _y = y;
    _type = type;
    _hp = hp;
    _max_hp = maxhp;
    _exp = exp;
    _level = level;
}

Object::Object(long long id, short x, short y, char type)
{
    ZeroMemory(name, sizeof(name));
    _id = id; _x = x; _y = y;
    _type = type;
    _max_hp = -1;
}

void Object::move(short px, short py, char dir)
{
    _x = px;
    _y = py;
    _dir = dir; // 이동 방향 업데이트
}

void Object::draw(HDC hdc, int offsetX, int offsetY)
{
    // 말이 들어갈 칸의 좌상단 좌표
    int tileLeft = offsetX + _x * PieceSize * 2;
    int tileTop = offsetY + _y * PieceSize * 2;

    // 칸의 중앙에 말 배치
    int centerX = tileLeft + PieceSize;
    int centerY = tileTop + PieceSize;

    HBRUSH brush;
    HPEN pen;
    COLORREF playerColor;

    // Player(type == 0) 이면 파란색
    // NPC(type == 1) 이면 붉은색
    playerColor = (_type == 1 ? RGB(255, 0, 0) : RGB(0, 0, 255));

    brush = CreateSolidBrush(playerColor);
    pen = CreatePen(PS_SOLID, 2, playerColor);

    //SelectObject(hdc, brush); // 브러시 선택
    SelectObject(hdc, pen);   // 펜 선택
    switch (_type)
    {
    case 0: {
        drawperson(hdc, centerX, centerY);

        // 아이디 표시
        SetBkMode(hdc, OPAQUE);
        COLORREF textColorBg = RGB(255, 255, 200);
        SetBkColor(hdc, textColorBg);

        std::wstringstream ssOffset;
        ssOffset << L"(" << _id << L")";

        std::wstring offsetText = ssOffset.str();
        TextOut(hdc, centerX - PieceSize, centerY - PieceSize, offsetText.c_str(), offsetText.length());

        break;
    }
    case 1: {
        drawslime(hdc, centerX, centerY);
        
        break;
    }
    default:
        break;
    }
  
    drawhpbar(hdc, centerX, centerY);
    
    DeleteObject(brush);
    DeleteObject(pen); // 펜도 삭제해야 합니다.
}

void Object::drawperson(HDC hdc, int centerX, int centerY)
{
    // --- 사람 그리기 시작 ---

    // 1. 머리 (원)
    // Ellipse 함수를 사용하여 원을 그립니다. 현재 선택된 브러시로 채워집니다.
    int headRadius = PieceSize / 4;
    Ellipse(hdc, centerX - headRadius, centerY - PieceSize / 2, centerX + headRadius, centerY - PieceSize / 2 + headRadius * 2);

    // 2. 몸통 (선)
    MoveToEx(hdc, centerX, centerY - PieceSize / 2 + headRadius * 2, NULL); // 머리 아래에서 시작
    LineTo(hdc, centerX, centerY + PieceSize / 4); // 몸통 끝까지

    // 3. 팔 (선)
    MoveToEx(hdc, centerX, centerY - PieceSize / 2 + headRadius * 2 + PieceSize / 8, NULL); // 몸통 위쪽에서 시작
    LineTo(hdc, centerX - PieceSize / 4, centerY + PieceSize / 8); // 왼쪽 팔
    MoveToEx(hdc, centerX, centerY - PieceSize / 2 + headRadius * 2 + PieceSize / 8, NULL); // 몸통 위쪽에서 다시 시작
    LineTo(hdc, centerX + PieceSize / 4, centerY + PieceSize / 8); // 오른쪽 팔

    // 4. 다리 (선)
    MoveToEx(hdc, centerX, centerY + PieceSize / 4, NULL); // 몸통 아래에서 시작
    LineTo(hdc, centerX - PieceSize / 6, centerY + PieceSize / 2); // 왼쪽 다리
    MoveToEx(hdc, centerX, centerY + PieceSize / 4, NULL); // 몸통 아래에서 다시 시작
    LineTo(hdc, centerX + PieceSize / 6, centerY + PieceSize / 2); // 오른쪽 다리

    // --- 사람 그리기 끝 ---
}

void Object::drawslime(HDC hdc, int centerX, int centerY)
{
    // --- 슬라임 그리기 시작 ---

    // 슬라임의 기본 형태 (타원 또는 둥근 사각형)
    // Ellipse 함수를 사용하여 슬라임의 몸통을 그립니다.
    // PieceSize를 기준으로 슬라임의 크기를 조정할 수 있습니다.
    int slimeWidth = PieceSize * 3 / 4;
    int slimeHeight = PieceSize * 2 / 3;

    // 슬라임 몸통 그리기 (중앙 하단을 기준으로)
    Ellipse(hdc, centerX - slimeWidth / 2, centerY - slimeHeight / 2,
        centerX + slimeWidth / 2, centerY + slimeHeight / 2);

    // 슬라임 눈 (원 2개)
    // 슬라임 몸통 위에 작은 원 두 개를 그립니다.
    int eyeRadius = PieceSize / 12;
    int eyeOffsetY = PieceSize / 8;
    int eyeOffsetX = PieceSize / 6;

    Ellipse(hdc, centerX - eyeOffsetX - eyeRadius, centerY - eyeOffsetY - eyeRadius,
        centerX - eyeOffsetX + eyeRadius, centerY - eyeOffsetY + eyeRadius); // 왼쪽 눈
    Ellipse(hdc, centerX + eyeOffsetX - eyeRadius, centerY - eyeOffsetY - eyeRadius,
        centerX + eyeOffsetX + eyeRadius, centerY - eyeOffsetY + eyeRadius); // 오른쪽 눈

    // --- 슬라임 그리기 끝 ---
}

void Object::drawhpbar(HDC hdc, int centerX, int centerY)
{
    if (_max_hp > 0)
    {
        // 체력바의 최대 너비 (캐릭터 너비에 비례)
        int barWidth = PieceSize; // PieceSize와 동일하게 설정 (조절 가능)
        int barHeight = 5;       // 체력바 높이

        // 체력바가 그려질 상단 좌표 (캐릭터 머리 위쪽)
        int barTop = centerY - PieceSize / 2 - barHeight - 5; // 캐릭터 머리 위, 여백 5픽셀
        int barLeft = centerX - barWidth / 2;
        int barRight = centerX + barWidth / 2;
        int barBottom = barTop + barHeight;

        // 체력바 배경 (빈 체력) - 검은색 또는 회색
        RECT bgRect = { barLeft, barTop, barRight, barBottom };
        HBRUSH bgBrush = CreateSolidBrush(RGB(50, 50, 50)); // 어두운 회색
        FillRect(hdc, &bgRect, bgBrush);
        DeleteObject(bgBrush);

        // 예시: 최대 체력이 100이라고 가정
        int currentHpWidth = (int)((double)_hp / _max_hp * barWidth);

        // 실제 체력 (초록색)
        RECT hpRect = { barLeft, barTop, barLeft + currentHpWidth, barBottom };
        HBRUSH hpBrush = CreateSolidBrush(RGB(0, 200, 0)); // 초록색
        FillRect(hdc, &hpRect, hpBrush);
        DeleteObject(hpBrush);
    }
}
