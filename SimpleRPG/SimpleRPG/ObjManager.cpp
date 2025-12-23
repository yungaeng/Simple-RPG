#include "ObjManager.h"

void ObjManager::Draw(HDC hdc, float offsetX, float offsetY)
{
    character.Draw(hdc, offsetX, offsetY);
}

void ObjManager::Update(float deltaTime)
{
    float baseSpeed = 200.0f;    // 기본 걷기 속도
    float maxSpeed = 500.0f;     // 최대 달리기 속도
    float acceleration = 400.0f; // 초당 증가할 속도 수치

    // 현재 오브젝트의 속도를 가져옵니다. 
    // (Object 클래스에 m_currentSpeed 멤버 변수가 있다고 가정)
    float currentSpeed = character.GetCurrentSpeed();

    float vx = 0, vy = 0;
    bool isMoving = false;

    // 1. 방향 입력 확인
    if (GetKeyState('W') & 0x8000) vy -= 1.0f;
    if (GetKeyState('S') & 0x8000) vy += 1.0f;
    if (GetKeyState('A') & 0x8000) vx -= 1.0f;
    if (GetKeyState('D') & 0x8000) vx += 1.0f;

    if (vx != 0 || vy != 0) isMoving = true;

    // 2. Shift 키 입력에 따른 속도 제어
    if (isMoving) {
        if (GetKeyState(VK_LSHIFT) & 0x8000 || GetKeyState(VK_RSHIFT) & 0x8000) // 왼쪽 Shift 누름
        {
            currentSpeed += acceleration * deltaTime; // 가속
            if (currentSpeed > maxSpeed)
                currentSpeed = maxSpeed;
        }
        else // Shift 뗌
        {
            if (currentSpeed > baseSpeed)
            {
                currentSpeed -= acceleration * deltaTime; // 감속
                if (currentSpeed < baseSpeed)
                    currentSpeed = baseSpeed;
            }
            else
                currentSpeed = baseSpeed;
        }
    }
    else
        currentSpeed = 0; // 이동하지 않으면 속도 0

    // 3. 실제 이동 처리
    if (isMoving) {
        // 대각선 정규화
        float length = sqrtf(vx * vx + vy * vy);
        vx /= length;
        vy /= length;

        float nextX = character.GetX() + (vx * currentSpeed * deltaTime);
        float nextY = character.GetY() + (vy * currentSpeed * deltaTime);

        character.SetPos(nextX, nextY);
    }

    // 4. 변화된 현재 속도를 오브젝트에 다시 저장
    character.SetcurrentSpeed(currentSpeed);
}