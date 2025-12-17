#include "Game.h"

void Game::Init()
{
	// 타이머 시작
	m_timer = std::chrono::steady_clock::now();
	
	world.Init();
}

void Game::Draw(HDC hdc)
{
	world.Draw(hdc);
	obj.Draw(hdc);
}

void Game::Input(WPARAM wParam, LPARAM lParam)
{
	// 키 입력 처리
	switch (wParam)
	{
	case 'Q': PostQuitMessage(0);
	}

	// 마우스 입력 처리
	float x = LOWORD(lParam);
	float y = HIWORD(lParam);
}

void Game::Update()
{
	// 1. 지난 프레임으로부터 흐른 시간 계산
	auto now = std::chrono::steady_clock::now();
	std::chrono::duration<float> duration = now - m_timer;
	float deltaTime = duration.count();

	// 2. 너무 미세한 시간(프레임 과다) 방지 (선택 사항)
	// 60FPS(0.016s) 정도로 제한하고 싶다면 아래 주석을 해제하세요.
	if (deltaTime < 0.016f) return; 

	// 3. 타이머를 현재 시간으로 갱신 (핵심!)
	m_timer = now;

	// 4. 오브젝트 업데이트
	obj.Update(deltaTime);
}

void Game::End()
{
	world.End();
}

double Game::GetElapsedTime() {
	auto now = std::chrono::steady_clock::now();
	auto duration = now - m_timer;
	return std::chrono::duration_cast<std::chrono::duration<double>>(duration).count();
}
