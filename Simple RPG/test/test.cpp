#include <iostream>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <memory>
#include <random> // 텔레포트를 위한 랜덤
#include <mutex> // send_lock을 위한 mutex

#include "..\Server\game_header.h"


#pragma comment (lib, "WS2_32.LIB")
#pragma comment (lib, "MSWSock.LIB")


// 패킷 정의 (서버의 game_header.h와 일치해야 함)
constexpr int BUF_SIZE = 265;


// IOCP 관련 구조체
enum IO_TYPE { IO_RECV, IO_SEND };
struct OVERLAPPED_EX {
    WSAOVERLAPPED over;
    IO_TYPE type;
    char buffer[BUF_SIZE];
    int buf_len;
};
struct CLIENT_SESSION {
    SOCKET sock;
    SOCKADDR_IN addr;
    char recv_buf[BUF_SIZE];
    int prev_recv_len;
    long long client_id; // 서버로부터 받은 ID
    int current_x, current_y; // 현재 위치 (서버 응답 기반)

    // Send Queue for this session
    std::mutex send_lock;
    std::vector<char> send_buffer_queue;

    CLIENT_SESSION() : sock(INVALID_SOCKET), prev_recv_len(0), client_id(-1), current_x(0), current_y(0) {}
};

// 전역 변수
HANDLE g_h_iocp;
std::vector<std::shared_ptr<CLIENT_SESSION>> g_sessions(MAX_USER);
std::atomic_int g_connected_clients = 0;
std::atomic_bool g_running = true;

// 텔레포트 패킷을 보내기 위한 난수 생성기
std::random_device g_rd;
std::mt19937 g_gen(g_rd());

// 함수 선언
void WorkerThread();
void StartRecv(int idx);
void HandlePacket(int idx, char* packet);
void SendPacket(int idx, const char* packet, int packet_size);
void SendLoginPacket(int idx);
void SendMovePacket(int idx, char direction);
void SendTeleportPacket(int idx); // 새로 추가

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    g_h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
    if (g_h_iocp == NULL) {
        std::cerr << "CreateIoCompletionPort failed.\n";
        WSACleanup();
        return 1;
    }

    // 워커 스레드 생성 (CPU 코어 수에 맞게 조절)
    const int num_worker_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> worker_threads;
    for (int i = 0; i < num_worker_threads; ++i) {
        worker_threads.emplace_back(WorkerThread);
    }

    std::cout << "Attempting to connect " << MAX_USER << " clients...\n";

    for (int i = 0; i < MAX_USER; ++i) {
        g_sessions[i] = std::make_shared<CLIENT_SESSION>();
        g_sessions[i]->sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
        if (g_sessions[i]->sock == INVALID_SOCKET) {
            std::cerr << "WSASocket failed for client " << i << ". Error: " << WSAGetLastError() << "\n";
            continue;
        }

        g_sessions[i]->addr.sin_family = AF_INET;
        g_sessions[i]->addr.sin_port = htons(GAME_PORT);
        inet_pton(AF_INET, "127.0.0.1", &g_sessions[i]->addr.sin_addr);

        // 동기식 connect 사용 (비동기 처리와 혼용)
        if (connect(g_sessions[i]->sock, reinterpret_cast<sockaddr*>(&g_sessions[i]->addr), sizeof(g_sessions[i]->addr)) == SOCKET_ERROR) {
            if (WSAGetLastError() != WSAEWOULDBLOCK) { // Non-blocking socket connect may return WSAEWOULDBLOCK
                std::cerr << "Failed to connect client " << i << ". Error: " << WSAGetLastError() << "\n";
                closesocket(g_sessions[i]->sock);
                g_sessions[i].reset();
                continue;
            }
        }

        // IOCP에 소켓 등록 (연결이 성공했거나 WSA_IO_PENDING 상태일 때만 등록)
        if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_sessions[i]->sock), g_h_iocp, (ULONG_PTR)i, 0) == NULL) {
            std::cerr << "CreateIoCompletionPort failed for client " << i << ". Error: " << WSAGetLastError() << "\n";
            closesocket(g_sessions[i]->sock);
            g_sessions[i].reset();
            continue;
        }

        g_connected_clients++;
        SendLoginPacket(i); // 연결 성공 시 로그인 패킷 전송
        SendTeleportPacket(i); // 로그인 직후 텔레포트 패킷 전송
        StartRecv(i); // 수신 시작
        Sleep(100);
    }

    std::cout << "Successfully connected " << g_connected_clients << " clients.\n";
    if (g_connected_clients == 0) {
        std::cerr << "No clients connected. Exiting.\n";
        g_running = false;
        for (auto& t : worker_threads) {
            if (t.joinable()) t.join();
        }
        CloseHandle(g_h_iocp);
        WSACleanup();
        return 1;
    }

    // 1초마다 이동 패킷 전송 루프
    std::cout << "Starting move packet simulation...\n";
    std::uniform_int_distribution<> distrib(0, 3); // 0:UP, 1:DOWN, 2:LEFT, 3:RIGHT

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // 1초 대기

        if (g_connected_clients == 0) {
            std::cout << "All clients disconnected. Exiting simulation.\n";
            g_running = false;
            break;
        }

        for (int i = 0; i < MAX_USER; ++i) {
            if (g_sessions[i] && g_sessions[i]->sock != INVALID_SOCKET && g_sessions[i]->client_id != -1) {
                char direction = distrib(g_gen); // 전역 난수 생성기 사용
                SendMovePacket(i, direction);
            }
        }
    }

    std::cout << "Simulation ended. Joining worker threads...\n";

    // 종료 시그널 전송 (NULL completion packet)
    for (int i = 0; i < num_worker_threads; ++i) {
        PostQueuedCompletionStatus(g_h_iocp, 0, 0, NULL);
    }

    for (auto& t : worker_threads) {
        if (t.joinable()) t.join();
    }

    for (int i = 0; i < MAX_USER; ++i) {
        if (g_sessions[i] && g_sessions[i]->sock != INVALID_SOCKET) {
            closesocket(g_sessions[i]->sock);
        }
    }

    CloseHandle(g_h_iocp);
    WSACleanup();

    std::cout << "Client program terminated.\n";
    return 0;
}

void WorkerThread() {
    DWORD bytes_transferred;
    ULONG_PTR completion_key;
    LPOVERLAPPED lpOverlapped;

    while (g_running) {
        BOOL ret = GetQueuedCompletionStatus(g_h_iocp, &bytes_transferred, &completion_key, &lpOverlapped, INFINITE);

        if (lpOverlapped == NULL) { // 종료 시그널
            g_running = false;
            break;
        }

        int client_idx = static_cast<int>(completion_key);
        if (client_idx < 0 || client_idx >= MAX_USER || !g_sessions[client_idx]) {
            delete reinterpret_cast<OVERLAPPED_EX*>(lpOverlapped);
            continue;
        }

        OVERLAPPED_EX* ov_ex = reinterpret_cast<OVERLAPPED_EX*>(lpOverlapped);
        std::shared_ptr<CLIENT_SESSION> session = g_sessions[client_idx];

        if (!ret || (bytes_transferred == 0 && (ov_ex->type == IO_RECV || ov_ex->type == IO_SEND))) {
            std::cerr << "Client " << client_idx << " disconnected (bytes_transferred: " << bytes_transferred << ", Error: " << GetLastError() << ").\n";
            closesocket(session->sock);
            session->sock = INVALID_SOCKET;
            g_connected_clients--;
            delete ov_ex;
            continue;
        }

        switch (ov_ex->type) {
        case IO_RECV: {
            session->prev_recv_len += bytes_transferred;
            char* p = session->recv_buf;

            while (session->prev_recv_len >= sizeof(short)) {
                short packet_size = *reinterpret_cast<short*>(p);
                if (session->prev_recv_len < packet_size) break; // 패킷이 불완전

                HandlePacket(client_idx, p);
                session->prev_recv_len -= packet_size;
                p += packet_size;
            }
            // 남은 데이터가 있다면 버퍼 앞으로 이동
            if (session->prev_recv_len > 0) {
                memmove(session->recv_buf, p, session->prev_recv_len);
            }
            StartRecv(client_idx); // 다음 수신 요청
            break;
        }
        case IO_SEND: {
            // 전송 완료 후 Send Queue 처리
            std::lock_guard<std::mutex> lock(session->send_lock);
            // 이미 보낸 데이터만큼 큐에서 제거
            if (bytes_transferred < session->send_buffer_queue.size()) {
                session->send_buffer_queue.erase(session->send_buffer_queue.begin(), session->send_buffer_queue.begin() + bytes_transferred);
            }
            else {
                session->send_buffer_queue.clear();
            }

            if (!session->send_buffer_queue.empty()) {
                // 큐에 데이터가 남아있으면 다시 전송 요청
                WSABUF wsa_buf;
                wsa_buf.buf = session->send_buffer_queue.data();
                wsa_buf.len = session->send_buffer_queue.size();

                DWORD flags = 0;
                DWORD sent_bytes;
                ZeroMemory(&ov_ex->over, sizeof(WSAOVERLAPPED)); // 재사용 전에 오버랩 초기화

                int ret_val = WSASend(session->sock, &wsa_buf, 1, &sent_bytes, flags, &ov_ex->over, NULL);
                if (ret_val == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
                    std::cerr << "WSASend failed for client " << client_idx << " on queue. Error: " << WSAGetLastError() << "\n";
                    closesocket(session->sock);
                    session->sock = INVALID_SOCKET;
                    g_connected_clients--;
                }
            }
            else {
                delete ov_ex; // 큐가 비었으면 OVERLAPPED_EX 객체 삭제
            }
            break;
        }
        }
    }
}

void StartRecv(int idx) {
    if (!g_sessions[idx] || g_sessions[idx]->sock == INVALID_SOCKET) return;

    OVERLAPPED_EX* ov_ex = new OVERLAPPED_EX();
    ZeroMemory(&ov_ex->over, sizeof(WSAOVERLAPPED));
    ov_ex->type = IO_RECV;
    ov_ex->buf_len = BUF_SIZE - g_sessions[idx]->prev_recv_len; // 남은 버퍼 공간

    WSABUF wsa_buf;
    wsa_buf.buf = g_sessions[idx]->recv_buf + g_sessions[idx]->prev_recv_len;
    wsa_buf.len = ov_ex->buf_len;

    DWORD flags = 0;
    DWORD recv_bytes;
    int ret_val = WSARecv(g_sessions[idx]->sock, &wsa_buf, 1, &recv_bytes, &flags, &ov_ex->over, NULL);
    if (ret_val == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        std::cerr << "WSARecv failed for client " << idx << ". Error: " << WSAGetLastError() << "\n";
        closesocket(g_sessions[idx]->sock);
        g_sessions[idx]->sock = INVALID_SOCKET;
        g_connected_clients--;
        delete ov_ex;
    }
}

void HandlePacket(int idx, char* packet) {
    std::shared_ptr<CLIENT_SESSION> session = g_sessions[idx];
    if (!session) return;

    short packet_size = *reinterpret_cast<short*>(packet);
    char packet_type = packet[sizeof(short)];

    switch (packet_type) {
    case S2C_P_AVATAR_INFO: {
        sc_packet_avatar_info* p = reinterpret_cast<sc_packet_avatar_info*>(packet);
        session->client_id = p->id;
        session->current_x = p->x;
        session->current_y = p->y;
        // std::cout << "Client " << idx << " logged in. ID: " << p->id << ", Name: " << p->name << ", Pos: (" << p->x << ", " << p->y << ")\n";
        break;
    }
    case S2C_P_MOVE: {
        sc_packet_move* p = reinterpret_cast<sc_packet_move*>(packet);
        if (p->id == session->client_id) { // 내 이동 패킷이라면 위치 업데이트
            session->current_x = p->x;
            session->current_y = p->y;
        }
        break;
    }
    case S2C_P_ENTER: {
        // std::cout << "ID: " << reinterpret_cast<sc_packet_enter*>(packet)->id << " entered view.\n";
        break;
    }
    case S2C_P_LEAVE: {
        // std::cout << "ID: " << reinterpret_cast<sc_packet_leave*>(packet)->id << " left view.\n";
        break;
    }
                    // 다른 패킷 타입 처리 (필요하다면)
    default:
        // std::cout << "Client " << idx << " received unknown packet type: " << (int)packet_type << "\n";
        break;
    }
}

void SendPacket(int idx, const char* packet, int packet_size) {
    if (!g_sessions[idx] || g_sessions[idx]->sock == INVALID_SOCKET) return;

    std::shared_ptr<CLIENT_SESSION> session = g_sessions[idx];
    std::lock_guard<std::mutex> lock(session->send_lock);

    // 큐에 데이터 추가
    session->send_buffer_queue.insert(session->send_buffer_queue.end(), packet, packet + packet_size);

    // 큐에 처음 데이터가 들어왔거나, 이전 전송이 완료된 경우에만 WSASend 호출
    // 이미 전송 요청이 진행 중인 경우는 큐에만 추가하고 WSASend는 WorkerThread에서 담당
    if (session->send_buffer_queue.size() == (size_t)packet_size) { // 큐에 방금 추가한 이 패킷만 있다면
        OVERLAPPED_EX* ov_ex = new OVERLAPPED_EX();
        ZeroMemory(&ov_ex->over, sizeof(WSAOVERLAPPED));
        ov_ex->type = IO_SEND;

        WSABUF wsa_buf;
        wsa_buf.buf = session->send_buffer_queue.data(); // 큐의 시작 지점
        wsa_buf.len = session->send_buffer_queue.size();

        DWORD flags = 0;
        DWORD sent_bytes;
        int ret_val = WSASend(session->sock, &wsa_buf, 1, &sent_bytes, flags, &ov_ex->over, NULL);
        if (ret_val == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            std::cerr << "WSASend failed for client " << idx << ". Error: " << WSAGetLastError() << "\n";
            closesocket(session->sock);
            session->sock = INVALID_SOCKET;
            g_connected_clients--;
            delete ov_ex;
        }
    }
}

void SendLoginPacket(int idx) {
    cs_packet_login p;
    p.size = sizeof(p);
    p.type = C2S_P_LOGIN;
    sprintf_s(p.name, "TestUser%04d", idx); // 고유한 이름
    SendPacket(idx, reinterpret_cast<char*>(&p), p.size);
}

void SendMovePacket(int idx, char direction) {
    cs_packet_move p;
    p.size = sizeof(p);
    p.type = C2S_P_MOVE;
    p.direction = direction;
    SendPacket(idx, reinterpret_cast<char*>(&p), p.size);
}

void SendTeleportPacket(int idx) {
    cs_packet_teleport p;
    p.size = sizeof(p);
    p.type = C2S_P_TELEPORT;
    SendPacket(idx, reinterpret_cast<char*>(&p), p.size);
}