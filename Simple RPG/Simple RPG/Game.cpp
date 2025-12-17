#include "Game.h"

void Game::Init(HWND hwnd)
{
	m_hWnd = hwnd;

	::CreateEvent(NULL, FALSE, FALSE, NULL);
}

void Game::Draw(HDC hdc)
{
	DrawBackGround(hdc);

	game_lock.lock();
	for(auto& f : o.m_foods)
		f.second.Draw(hdc);

	for(auto& s : o.m_snakes)
		s.second.Draw(hdc);
	game_lock.unlock();
}

void Game::ReStart()
{
	m_userdata.score = 0;
	m_isgameover = false;
	SendRestart();
}

void Game::StartBGM()
{
	PlaySound(L"bgm.wav", NULL, SND_ASYNC | SND_LOOP | SND_FILENAME);
}
void Game::StopBGM()
{
	PlaySound(NULL, NULL, 0);
}

bool Game::InitNetwork()
{
	// 윈속 초기화
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		MessageBox(NULL, L"WSADATA Init Error", L"Error", MB_ICONERROR);
		return false;
	}

	// 소켓 생성
	m_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(m_socket == INVALID_SOCKET) {
		MessageBox(NULL, L"Socket Error", L"Error", MB_ICONERROR);
		return false;
	}

	/*u_long non_blocking_mode = 1;
	if(ioctlsocket(m_socket, FIONBIO, &non_blocking_mode) == SOCKET_ERROR) {
		MessageBox(NULL, L"Non-Blocking Error!", L"Error", MB_ICONERROR);
		return false;
	}*/

	// connect()
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	inet_pton(AF_INET, SERVER_IP, &serveraddr.sin_addr);
	serveraddr.sin_port = htons(SERVER_PORT);
	int retval = connect(m_socket, (struct sockaddr*)&serveraddr, sizeof(serveraddr));

	if (retval == SOCKET_ERROR) {
		int errCode = WSAGetLastError();
		if (errCode != WSAEWOULDBLOCK) {
			// ���� �õ� ��ü�� �Ұ����� �ɰ��� ����
			MessageBox(NULL, L"Connect Error!", L"Error", MB_ICONERROR);
			return false;
		}
		else {
			std::this_thread::sleep_for(2s);
		};
	}

	m_isconnect = true;
	std::thread([this]() { Recv(); }).detach();
	return true;
}
void Game::Recv()
{
	while(m_isconnect) {
		int recvLen = ::recv(m_socket, m_recv_buf + m_received_bytes, BUF_SIZE - m_received_bytes, 0);

		if(recvLen == 0) {
			// 연결 종료
			EndNetwork();
			break;
		}
		else if(recvLen < 0) {
			// 오류 처리 (넌블로킹 소켓이므로 WSAEWOULDBLOCK은 정상)
			int errCode = WSAGetLastError();
			if(errCode != WSAEWOULDBLOCK) {
				EndNetwork();
				break;
			}
			// WSAEWOULDBLOCK이 발생하면 잠시 쉬고 다시 시도 (메인 스레드 블로킹 방지)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue; // 재시도
		}

		// 2. 수신 성공: 누적 바이트 업데이트
		m_received_bytes += recvLen;

		// 3. 패킷 조립 및 처리 루프 (이전 논블로킹 로직 복구)
		while(m_received_bytes > 0) {
			// 헤더가 불완전하면 다음 recv를 기다립니다.
			if(m_received_bytes < sizeof(PacketHeader)) {
				break;
			}

			PacketHeader* header = reinterpret_cast<PacketHeader*>(m_recv_buf);
			uint8_t totalSize = header->packetSize;

			// [추가된 안전 검사]
			if(totalSize == 0 || totalSize > BUF_SIZE || totalSize < sizeof(PacketHeader)) {
				// 심각한 오류로 간주하고 연결 종료
				MessageBox(NULL, L"Corrupt Packet Size!", L"Error", MB_ICONERROR);
				EndNetwork();
				break; // Recv 루프 종료
			}

			// 완전한 패킷이 도착했는지 확인
			if(m_received_bytes < totalSize) {
				break;
			}

			// 4. 완전한 패킷 처리
			ProcessPacket(m_recv_buf);

			// 5. 처리된 패킷만큼 버퍼 이동 및 잔여 바이트 갱신
			m_received_bytes -= totalSize;
			if(m_received_bytes > 0) {
				memmove(m_recv_buf, m_recv_buf + totalSize, m_received_bytes);
			}
		}
	}

	// 루프 종료 시 네트워크 정리
	EndNetwork();
}
void Game::ProcessPacket(char* data)
{
	// 1. 패킷 헤더에서 ID 추출
	PacketHeader* header = reinterpret_cast<PacketHeader*>(data);
	PACKET_ID pid = static_cast<PACKET_ID>(header->packetID);

	// 2. 패킷 ID에 따라 적절히 처리
	switch(pid) {
		case PACKET_ID::S2C_LOGIN_OK:
		{
			S2C_LOGIN_OK_PACKET* p = reinterpret_cast<S2C_LOGIN_OK_PACKET*>(data);
			game_lock.lock();
			o.AddSnake(p->id, m_userdata.name, p->x, p->y, m_userdata.color);
			game_lock.unlock();

			// 본인 아이디 저장
			m_userdata.id = p->id;
			SetLogin(true);
			break;
		}
		case PACKET_ID::S2C_LOGIN_FAIL:
		{
			SetLogin(false);
			break;
		}
		case PACKET_ID::S2C_SNAKE:
		{
			S2C_SNAKE_PACKET* p = reinterpret_cast<S2C_SNAKE_PACKET*>(data);
			game_lock.lock();
			o.AddSnake(p->id, p->name, p->x, p->y, p->color);
			game_lock.unlock();
			break;
		}
		case PACKET_ID::S2C_FOOD:
		{
			S2C_FOOD_PACKET* p = reinterpret_cast<S2C_FOOD_PACKET*>(data);
			game_lock.lock();
			o.AddFood(p->id, p->x, p->y, p->color);
			game_lock.unlock();
			break;
		}
		case PACKET_ID::S2C_MOVE:
		{
			S2C_MOVE_PACKET* p = reinterpret_cast<S2C_MOVE_PACKET*>(data);
			game_lock.lock();
			if(o.m_snakes.contains(p->id))
				o.m_snakes[p->id].m_head.SetPos(p->x, p->y);
			game_lock.unlock();
			break;
		}
		case PACKET_ID::S2C_SNAKE_BODY:
		{
			S2C_SNAKE_BODY_PACKET* p = reinterpret_cast<S2C_SNAKE_BODY_PACKET*>(data);
			game_lock.lock();
			if(o.m_snakes.contains(p->id))
				o.m_snakes[p->id].SetBody(p->bodyIndex, p->x, p->y);
			game_lock.unlock();
			break;
		}
		case PACKET_ID::S2C_DEL_SNAKE:
		{
			S2C_DEL_SNAKE_PACKET* p = reinterpret_cast<S2C_DEL_SNAKE_PACKET*>(data);
			game_lock.lock();
			o.DeleteSnake(p->id);
			// 내 아이디이면 게임오버
			if(m_userdata.id == p->id) {
				m_isgameover = true;
			}
			game_lock.unlock();
			break;
		}
		case PACKET_ID::S2C_DEL_FOOD:
		{
			S2C_DEL_FOOD_PACKET* p = reinterpret_cast<S2C_DEL_FOOD_PACKET*>(data);
			game_lock.lock();
			o.DeleteFood(p->id);
			game_lock.unlock();
			break;
		}
		case PACKET_ID::S2C_ADD_SNAKE_BODY:
		{
			S2C_ADD_SNAKE_BDOY_PACKET* p = reinterpret_cast<S2C_ADD_SNAKE_BDOY_PACKET*>(data);
			game_lock.lock();
			if(o.m_snakes.contains(p->id)) {
				o.m_snakes[p->id].AddBody(p->bodyIndex);
				o.m_snakes[p->id].SetBody(p->bodyIndex, p->x, p->y);
			}
			if(p->id == m_userdata.id) {
				if(o.m_snakes[p->id].GetBody().size() >= 2)
					m_userdata.score += 10;
			}
			//o.m_snakes[p->id].AddBody(p->x,p->y);
			game_lock.unlock();

			break;
		}
		default:
		{
			// 알 수 없는 패킷을 받았으므로 연결을 끊거나 무시할 수 있습니다.
			break;
		}
	}
}

bool Game::Connect()
{
	char char_ip[64];
	int size_needed = WideCharToMultiByte(CP_ACP, 0, m_ip, -1, char_ip, 64, NULL, NULL);

	// connect()
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	inet_pton(AF_INET, char_ip, &serveraddr.sin_addr);
	serveraddr.sin_port = htons(SERVER_PORT);
	int retval = connect(m_socket, (struct sockaddr*)&serveraddr, sizeof(serveraddr));

	if(retval == SOCKET_ERROR) {
		int errCode = WSAGetLastError();
		if(errCode != WSAEWOULDBLOCK) {
			// 연결 시도 자체가 불가능한 심각한 오류
			MessageBox(NULL, L"Connect Error!", L"Error", MB_ICONERROR);
			return false;
		}
		else {
			// WSAEWOULDBLOCK: 연결 시도가 진행 중임 (정상적인 논블로킹 동작)
			std::this_thread::sleep_for(1s);
		}
	}
	m_isconnect = true; // 연결 시도가 시작되었으므로 true
	SetEvent(m_eveHandle);
	std::thread([this]() { Recv(); }).detach();

	return true;
}

void Game::SendLogin()
{
	if(m_isconnect) {
		C2S_LOGIN_PACKET sendPkt = {};
		memcpy(sendPkt.name, m_userdata.name, sizeof(m_userdata.name));
		sendPkt.color = m_userdata.color;
		send(m_socket, (char*)&sendPkt, sizeof(sendPkt), 0);
	}
}
void Game::SendMove(float x, float y)
{
	if(m_isconnect && m_islogin) {
		C2S_MOVE_PACKET sendPkt = {};
		sendPkt.x = x; sendPkt.y = y;
		memcpy(m_send_buf, &sendPkt, sizeof(sendPkt));
		send(m_socket, (char*)&sendPkt, sizeof(sendPkt), 0);
	}
}
void Game::SendRestart()
{
	if(m_isconnect) {
		C2S_RESTART_PACKET sendPkt = {};
		memcpy(m_send_buf, &sendPkt, sizeof(sendPkt));
		send(m_socket, (char*)&sendPkt, sizeof(sendPkt), 0);
	}
}

void Game::EndNetwork()
{
	m_isconnect = false;
	// 소켓 닫기
	closesocket(m_socket);
	WSACleanup();
}

void Game::DrawBackGround(HDC hdc)
{
	game_lock.lock();

	RECT backgroundRect = { 0, 0, 1024, 768 };
	HBRUSH backgroundBrush = CreateSolidBrush(RGB(10, 10, 10));
	FillRect(hdc, &backgroundRect, backgroundBrush);
	DeleteObject(backgroundBrush);

	// --- 텍스트 출력 추가 부분 ---
	char textBuffer[50];
	sprintf_s(textBuffer, sizeof(textBuffer), "FOODS : %d | SNAKES : %d | SCORE : %d", (int)o.m_foods.size(), (int)o.m_snakes.size(), m_userdata.score);
	SetBkMode(hdc, OPAQUE);
	COLORREF textColorBg = RGB(255, 255, 200);
	SetBkColor(hdc, textColorBg);
	TextOutA(hdc, 10, 10, textBuffer, (int)strlen(textBuffer));
	game_lock.unlock();
}
