#include "ObjManager.h"

void ObjManager::MoveObj(char dir)
{
	float x = obj.GetX();
	float y = obj.GetY();
	float speed = obj.GetSpeed();
	switch (dir)
	{
	case 'W':
		obj.SetPos(x, y - speed);
		break;
	case 'A':
		obj.SetPos(x - speed, y);
		break;
	case 'S':
		obj.SetPos(x, y + speed);
		break;
	case 'D':
		obj.SetPos(x + speed, y);
		break;
	default:
		break;
	}
}

void ObjManager::Draw(HDC hdc)
{
	obj.Draw(hdc);
}

void ObjManager::Update(float deltaTime)
{
	float speed = obj.GetSpeed(); // 예: 300.0f (픽셀/초)
	float vx = 0, vy = 0;

	// GetKeyState를 사용하여 여러 키가 동시에 눌린 것을 확인
	if (GetKeyState('W') & 0x8000) vy -= 1.0f;
	if (GetKeyState('S') & 0x8000) vy += 1.0f;
	if (GetKeyState('A') & 0x8000) vx -= 1.0f;
	if (GetKeyState('D') & 0x8000) vx += 1.0f;

	if (vx != 0 || vy != 0)
	{
		// 대각선 이동 속도 보정 (정규화)
		float length = sqrtf(vx * vx + vy * vy);
		vx /= length;
		vy /= length;

		// 결과 반영: 좌표 + (방향 * 속도 * 시간)
		float nextX = obj.GetX() + (vx * speed * deltaTime);
		float nextY = obj.GetY() + (vy * speed * deltaTime);

		obj.SetPos(nextX, nextY);
	}
}