#define _WINSOCK_DEPRECATED_NO_WARNINGS

#define NOMINMAX

#include <WinSock2.h>
#include <winsock.h>
#include <Windows.h>
#include <iostream>
#include <thread>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <chrono>
#include <queue>
#include <array>
#include <memory>


using namespace std;
using namespace chrono;

extern HWND		hWnd;

const static int MAX_TEST = 5000;
const static int MAX_CLIENTS = MAX_TEST * 2;
const static int INVALID_ID = -1;
const static int MAX_PACKET_SIZE = 255;
const static int MAX_BUFF_SIZE = 255;

#pragma comment (lib, "ws2_32.lib")

#include "..\Server\game_header.h"

HANDLE g_hiocp;

enum OPTYPE { OP_SEND, OP_RECV, OP_DO_MOVE };

high_resolution_clock::time_point last_connect_time;

struct OverlappedEx {
	WSAOVERLAPPED over;
	WSABUF wsabuf;
	unsigned char IOCP_buf[MAX_BUFF_SIZE];
	OPTYPE event_type;
	int event_target;
};

struct CLIENT {
	int id;
	int x;
	int y;
	atomic_bool connected;

	SOCKET client_socket;
	OverlappedEx recv_over;
	unsigned char packet_buf[MAX_PACKET_SIZE];
	int prev_packet_data;
	int curr_packet_size;
	high_resolution_clock::time_point last_move_time;
};

array<int, MAX_CLIENTS> client_map;
array<CLIENT, MAX_CLIENTS> g_clients;
atomic_int num_connections;
atomic_int client_to_close;
atomic_int active_clients;

int			global_delay;				// ms단위, 1000이 넘으면 클라이언트 증가 종료

vector <thread*> worker_threads;
thread test_thread;

float point_cloud[MAX_TEST * 2];

// 나중에 NPC까지 추가 확장 용
struct ALIEN {
	int id;
	int x, y;
	int visible_count;
};

void error_display(const char* msg, int err_no)
{
	WCHAR* lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	std::cout << msg;
	std::wcout << L"에러" << lpMsgBuf << std::endl;

	MessageBox(hWnd, lpMsgBuf, L"ERROR", 0);
	LocalFree(lpMsgBuf);
	// while (true);
}

void DisconnectClient(int ci)
{
	if (ci < 0 || ci >= MAX_CLIENTS) return;
	bool expected = true;
	// use the member compare_exchange_strong on atomic_bool
	if (g_clients[ci].connected.compare_exchange_strong(expected, false)) {
		// close socket and decrement active counter
		SOCKET s = g_clients[ci].client_socket;
		if (s != INVALID_SOCKET) {
			closesocket(s);
			g_clients[ci].client_socket = INVALID_SOCKET;
		}
		--active_clients;
		// reset client state
		g_clients[ci].id = INVALID_ID;
	}
}

void SendPacket(int cl, void* packet)
{
	if (cl < 0 || cl >= MAX_CLIENTS) return;
	int psize = reinterpret_cast<unsigned char*>(packet)[0];
	if (psize <= 0 || psize > MAX_PACKET_SIZE) return;

	OverlappedEx* over = new OverlappedEx;
	ZeroMemory(over, sizeof(OverlappedEx));
	over->event_type = OP_SEND;
	memcpy(over->IOCP_buf, packet, psize);
	over->wsabuf.buf = reinterpret_cast<CHAR*>(over->IOCP_buf);
	over->wsabuf.len = psize;

	int ret = WSASend(g_clients[cl].client_socket, &over->wsabuf, 1, NULL, 0,
		&over->over, NULL);
	if (0 == ret) {
		// Completed synchronously. Post a completion so worker thread will handle cleanup.
		PostQueuedCompletionStatus(g_hiocp, static_cast<DWORD>(over->wsabuf.len),
			static_cast<ULONG_PTR>(cl),
			reinterpret_cast<LPOVERLAPPED>(over));
	}
	else {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no) {
			// real error
			error_display("Error in SendPacket:", err_no);
			// ensure deletion if it won't be completed
			delete over;
		}
	}
}

void ProcessPacket(int ci, unsigned char packet[])
{
	switch (packet[1]) {
	case S2C_P_MOVE: {
		sc_packet_move* move_packet = reinterpret_cast<sc_packet_move*>(packet);
		if (move_packet->id < MAX_CLIENTS) {
			int my_id = client_map[move_packet->id];
			if (-1 != my_id) {
				g_clients[my_id].x = move_packet->x;
				g_clients[my_id].y = move_packet->y;
			}
			if (ci == my_id) {
				if (0 != move_packet->move_time) {
					auto d_ms = duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count() - move_packet->move_time;

					if (global_delay < d_ms) global_delay++;
					else if (global_delay > d_ms) global_delay--;
				}
			}
		}
		break;
	} 
	case S2C_P_LEAVE: break;
	case S2C_P_ENTER: break;
	case S2C_P_AVATAR_INFO:
	{
		g_clients[ci].connected = true;
		active_clients++;
		sc_packet_avatar_info* login_packet = reinterpret_cast<sc_packet_avatar_info*>(packet);
		int my_id = ci;
		client_map[login_packet->id] = my_id;
		g_clients[my_id].id = login_packet->id;
		g_clients[my_id].x = login_packet->x;
		g_clients[my_id].y = login_packet->y;

		cs_packet_teleport t_packet;
		t_packet.size = sizeof(t_packet);
		t_packet.type = C2S_P_TELEPORT;
		SendPacket(my_id, &t_packet);
		break;
	}
	default:
		break;
	}
}

// Add these globals (near other globals)
atomic_bool g_running{ true };

void Worker_Thread()
{
	while (g_running.load()) {
		DWORD io_size = 0;
		ULONG_PTR key = 0;
		OverlappedEx* over = nullptr;
		BOOL ret = GetQueuedCompletionStatus(g_hiocp, &io_size, &key,
			reinterpret_cast<LPOVERLAPPED*>(&over), INFINITE);
		int client_id = static_cast<int>(key);

		if (!ret) {
			int err_no = WSAGetLastError();
			if (over == nullptr) {
				// No overlapped provided. Nothing to delete; continue
				if (err_no == ERROR_ABANDONED_WAIT_0) break; // IOCP closed
				// attempt to disconnect client if key valid
				if (client_id >= 0 && client_id < MAX_CLIENTS)
					DisconnectClient(client_id);
				continue;
			}
			// If ret == FALSE but we have an overlapped, handle according to event type.
			if (over->event_type == OP_SEND) {
				// failed send, drop client
				DisconnectClient(client_id);
				delete over;
				continue;
			}
			if (io_size == 0) {
				DisconnectClient(client_id);
				if (over) delete over;
				continue;
			}
		}

		if (io_size == 0) {
			// graceful close
			DisconnectClient(client_id);
			if (over) delete over;
			continue;
		}

		if (over == nullptr) {
			// spurious; continue
			continue;
		}

		if (over->event_type == OP_RECV) {
			unsigned char* buf = g_clients[client_id].recv_over.IOCP_buf;
			unsigned psize = g_clients[client_id].curr_packet_size;
			unsigned pr_size = g_clients[client_id].prev_packet_data;
			unsigned char* local_buf = buf;
			DWORD remaining = io_size;
			while (remaining > 0) {
				if (psize == 0) psize = static_cast<unsigned char>(local_buf[0]);
				if (remaining + pr_size >= psize) {
					unsigned char packet[MAX_PACKET_SIZE];
					memcpy(packet, g_clients[client_id].packet_buf, pr_size);
					memcpy(packet + pr_size, local_buf, psize - pr_size);
					ProcessPacket(client_id, packet);
					local_buf += (psize - pr_size);
					remaining -= (psize - pr_size);
					psize = 0; pr_size = 0;
				}
				else {
					memcpy(g_clients[client_id].packet_buf + pr_size, local_buf, remaining);
					pr_size += remaining;
					remaining = 0;
				}
			}

			g_clients[client_id].curr_packet_size = psize;
			g_clients[client_id].prev_packet_data = pr_size;

			DWORD recv_flag = 0;
			int r = WSARecv(g_clients[client_id].client_socket,
				&g_clients[client_id].recv_over.wsabuf, 1,
				NULL, &recv_flag, &g_clients[client_id].recv_over.over, NULL);
			if (SOCKET_ERROR == r) {
				int eno = WSAGetLastError();
				if (eno != WSA_IO_PENDING) {
					DisconnectClient(client_id);
				}
			}
			// Note: the OverlappedEx used for recv is part of CLIENT struct; do not delete here.
		}
		else if (over->event_type == OP_SEND) {
			// Completed send — check size match
			if (io_size != over->wsabuf.len) {
				DisconnectClient(client_id);
			}
			delete over;
		}
		else if (over->event_type == OP_DO_MOVE) {
			// not implemented — clean up
			delete over;
		}
		else {
			// unknown event; cleanup and continue
			delete over;
		}
	}

	// exit
}

constexpr int DELAY_LIMIT = 100;
constexpr int DELAY_LIMIT2 = 150;
constexpr int ACCEPT_DELY = 50;

void Adjust_Number_Of_Client()
{
	static int delay_multiplier = 1;
	static int max_limit = MAXINT;
	static bool increasing = true;

	if (active_clients >= MAX_TEST) return;
	if (num_connections >= MAX_CLIENTS) return;

	auto duration = high_resolution_clock::now() - last_connect_time;
	if (ACCEPT_DELY * delay_multiplier > duration_cast<milliseconds>(duration).count()) return;

	int t_delay = global_delay;
	if (DELAY_LIMIT2 < t_delay) {
		if (true == increasing) {
			max_limit = active_clients;
			increasing = false;
		}
		if (100 > active_clients) return;
		if (ACCEPT_DELY * 10 > duration_cast<milliseconds>(duration).count()) return;
		last_connect_time = high_resolution_clock::now();
		DisconnectClient(client_to_close);
		client_to_close++;
		return;
	}
	else
		if (DELAY_LIMIT < t_delay) {
			delay_multiplier = 10;
			return;
		}
	if (max_limit - (max_limit / 20) < active_clients) return;

	increasing = true;
	last_connect_time = high_resolution_clock::now();
	g_clients[num_connections].client_socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN ServerAddr;
	ZeroMemory(&ServerAddr, sizeof(SOCKADDR_IN));
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(GAME_PORT);
	ServerAddr.sin_addr.s_addr = inet_addr("127.0.0.1");


	int Result = WSAConnect(g_clients[num_connections].client_socket, (sockaddr*)&ServerAddr, sizeof(ServerAddr), NULL, NULL, NULL, NULL);
	if (0 != Result) {
		error_display("WSAConnect : ", GetLastError());
	}

	g_clients[num_connections].curr_packet_size = 0;
	g_clients[num_connections].prev_packet_data = 0;
	ZeroMemory(&g_clients[num_connections].recv_over, sizeof(g_clients[num_connections].recv_over));
	g_clients[num_connections].recv_over.event_type = OP_RECV;
	g_clients[num_connections].recv_over.wsabuf.buf =
		reinterpret_cast<CHAR*>(g_clients[num_connections].recv_over.IOCP_buf);
	g_clients[num_connections].recv_over.wsabuf.len = sizeof(g_clients[num_connections].recv_over.IOCP_buf);

	DWORD recv_flag = 0;
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_clients[num_connections].client_socket), g_hiocp, num_connections, 0);

	cs_packet_login l_packet;

	int temp = num_connections;
	sprintf_s(l_packet.name, "%d", temp);
	l_packet.size = sizeof(l_packet);
	l_packet.type = C2S_P_LOGIN;
	SendPacket(num_connections, &l_packet);


	int ret = WSARecv(g_clients[num_connections].client_socket, &g_clients[num_connections].recv_over.wsabuf, 1,
		NULL, &recv_flag, &g_clients[num_connections].recv_over.over, NULL);
	if (SOCKET_ERROR == ret) {
		int err_no = WSAGetLastError();
		if (err_no != WSA_IO_PENDING)
		{
			error_display("RECV ERROR", err_no);
			goto fail_to_connect;
		}
	}
	num_connections++;
fail_to_connect:
	return;
}

void Test_Thread()
{
	while (true) {
		Sleep(max(20, global_delay));
		Adjust_Number_Of_Client();

		for (int i = 0; i < num_connections; ++i) {
			if (false == g_clients[i].connected) continue;
			if (g_clients[i].last_move_time + 1s > high_resolution_clock::now()) continue;
			g_clients[i].last_move_time = high_resolution_clock::now();
			cs_packet_move my_packet;
			my_packet.size = sizeof(my_packet);
			my_packet.type = C2S_P_MOVE;
			switch (rand() % 4) {
			case 0: my_packet.direction = 0; break;
			case 1: my_packet.direction = 1; break;
			case 2: my_packet.direction = 2; break;
			case 3: my_packet.direction = 3; break;
			}
			my_packet.move_time = static_cast<unsigned>(duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count());
			SendPacket(i, &my_packet);
		}
	}
}

void InitializeNetwork()
{
	for (auto& cl : g_clients) {
		cl.connected = false;
		cl.id = INVALID_ID;
		cl.client_socket = INVALID_SOCKET;
	}

	for (auto& cl : client_map) cl = -1;
	num_connections = 0;
	last_connect_time = high_resolution_clock::now();
	global_delay = 0;
	g_running = true;

	WSADATA wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	g_hiocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	unsigned int thread_count = std::max(2u, std::thread::hardware_concurrency());
	for (unsigned int i = 0; i < thread_count; ++i)
		worker_threads.push_back(new std::thread{ Worker_Thread });

	test_thread = thread{ Test_Thread };
}

// ShutdownNetwork: signal stop, wake threads, join, cleanup
void ShutdownNetwork()
{
	// signal stop
	g_running = false;

	// Wake all worker threads by posting empty completions
	for (size_t i = 0; i < worker_threads.size(); ++i) {
		PostQueuedCompletionStatus(g_hiocp, 0, 0, NULL);
	}

	// also wake Test_Thread by letting g_running false (its loop should check g_running)
	if (test_thread.joinable()) test_thread.join();

	// Wait workers
	for (auto pth : worker_threads) {
		if (pth->joinable()) pth->join();
		delete pth;
	}
	worker_threads.clear();

	// Close IOCP handle
	if (g_hiocp) {
		CloseHandle(g_hiocp);
		g_hiocp = NULL;
	}

	// Close any open sockets
	for (int i = 0; i < num_connections; ++i) {
		if (g_clients[i].client_socket != INVALID_SOCKET) {
			closesocket(g_clients[i].client_socket);
			g_clients[i].client_socket = INVALID_SOCKET;
		}
	}

	WSACleanup();
}

void Do_Network()
{
	return;
}

void GetPointCloud(int* size, float** points)
{
	int index = 0;
	for (int i = 0; i < num_connections; ++i)
		if (true == g_clients[i].connected) {
			point_cloud[index * 2] = static_cast<float>(g_clients[i].x);
			point_cloud[index * 2 + 1] = static_cast<float>(g_clients[i].y);
			index++;
		}

	*size = index;
	*points = point_cloud;
}

