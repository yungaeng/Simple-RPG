#pragma once

#define NOMINMAX

//Network
#include <winsock2.h>			
#include <ws2tcpip.h>
#include <mmsystem.h>
//#include <chrono>
//#include <cstdlib> // wcstombs

#pragma comment(lib, "ws2_32")

//#include "..\SnakeGame-Server\Server\pch.h"
#include "ObjManager.h"

#define SERVER_IP "192.168.147.130"
#define SERVER_PORT 9000
#define BUF_SIZE 512
#define MAX_NAME_SIZE 20

struct userdata {
	wchar_t name[MAX_NAME_SIZE];
	COLORREF color;
	unsigned long long id;
	unsigned int score;
};

class Game {
	wchar_t m_ip[64];
	bool m_isconnect = true;
	bool m_islogin = false;
	bool m_isgameover = false;

	SOCKET m_socket = {};
	char m_send_buf[BUF_SIZE];
	char m_recv_buf[BUF_SIZE];
	int m_received_bytes = 0;

	ObjManager o = {};
	userdata m_userdata = {};
	std::mutex game_lock;
	HWND m_hWnd;

public:
	HANDLE m_eveHandle;

public:
	Game()
	{
		m_isconnect = false;
		m_islogin = false;
		m_isgameover = false;

		m_socket = INVALID_SOCKET;
		m_send_buf[0] = '\0';
		m_recv_buf[0] = '\0';
		m_userdata = {};
	};
	~Game() {};
	void Init(HWND hwnd);
	void Draw(HDC hdc);
	void Input(WPARAM wParam, LPARAM lParam)
	{
		switch (wParam)
		{
		case 'Q': PostQuitMessage(0);
		}

		float x = LOWORD(lParam);
		float y = HIWORD(lParam);
		SendMove(x, y);
	};

	void Update();
	void ReStart();

	void StartBGM();
	void StopBGM();

	void SetUserdata(userdata ud) { m_userdata = ud; };
	// bool IsOneConnect() { return m_oneConnect; }

	userdata* GetUserdata() { return &m_userdata; };	
	wchar_t* GetNameById(int id) { return o.m_snakes[id].GetName(); };
	int GetScoreById(int id) { return (int)o.m_snakes[id].m_body.size() * 10; };
	

	//  Network --------------------------------------------------------------------------
	void SetIP(const wchar_t* ip) { memcpy(m_ip, ip, sizeof(m_ip)); };
	bool InitNetwork();
	void Recv();
	void ProcessPacket(char* data);
	bool Connect();

	void SendLogin();
	void SendMove(float x, float y);
	void SendRestart();
	
	void EndNetwork();

	bool GetConnect() { return m_isconnect; };
	bool GetLogin() { return m_islogin; };
	bool GetGameover() { return m_isgameover; };

private:
	void SetLogin(bool st) {
		m_islogin = st;
	};
	void DrawBackGround(HDC hdc);
};