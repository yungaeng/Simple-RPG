//#pragma once
//#include <WS2tcpip.h>
//#include <MSWSock.h>
//#include <iostream>
//#include <thread>
//#include <vector>
//#include <string>
//#include <locale>
//#include <codecvt> // For std::wstring_convert (C++11, but deprecated in C++17)
//#include <memory> // For std::unique_ptr
//#include <windows.h> 
//
//#include "..\Server\game_header.h"
//#include "Map.h"
//
//#pragma comment (lib, "WS2_32.LIB")
//#pragma comment (lib, "MSWSock.LIB")
//
//// 사용자 정의 메시지 (WM_USER + 임의의 값)
//#define WM_USER_UPDATE_DISPLAY (WM_USER + 1)
//#define WM_USER_APPEND_CHAT    (WM_USER + 2) // 채팅 메시지 추가를 위한 사용자 정의 메시지
//
//
//SOCKET client_socket = INVALID_SOCKET;
//Map map;
//int viewport_offset_x = SCREEN_WIDTH / 2;
//int viewport_offset_y = SCREEN_HEIGHT / 2;
//bool is_connected = false;
//long long myid = -1;
//constexpr int BUF_SIZE = 200;
//
//// 채팅 관련 컨트롤 핸들
//HWND hChatEdit;    // 채팅 입력창 (Edit Control)
//HWND hChatDisplay; // 채팅 출력창 (Edit Control 또는 ListBox)
//HWND g_hWnd; // Global HWND to access from recv_thread
//
//// 패킷 처리를 더 안정적으로 하기 위해 패킷 버퍼와 오프셋을 관리합니다.
//char g_recv_buffer[4096]; // 충분히 큰 수신 버퍼
//int g_packet_offset = 0; // 현재까지 받은 데이터의 시작 오프셋
//
//std::thread recvThread; // Make recvThread a global or member variable to manage its lifecycle
//
//// Forward declarations
//void process_packet(char* packet);
//void handle_packet(char* packet, int bytes_received);
//void append_chat_message(const WCHAR* message); // Helper for displaying messages in chat window
//void disconnect_client();
//bool connect_to_server(const char* ip_address);
//void send_login(const char* name);
//void send_move(const char dir);
//void send_attack();
//void send_chat(const char* message);
//
//
//// Helper for displaying messages in chat window
//void append_chat_message(const WCHAR* message) {
//    if (!hChatDisplay) return;
//
//    int nLen = GetWindowTextLength(hChatDisplay);
//    SendMessage(hChatDisplay, EM_SETSEL, nLen, nLen); // Move cursor to end
//    SendMessage(hChatDisplay, EM_REPLACESEL, FALSE, (LPARAM)message);
//
//    // Add a new line if not already there
//    if (wcslen(message) > 0 && message[wcslen(message) - 1] != L'\n') {
//        SendMessage(hChatDisplay, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
//    }
//
//    SendMessage(hChatDisplay, EM_SCROLLCARET, 0, 0); // Auto-scroll
//}
//void handle_packet(char* packet, int bytes_received)
//{
//    // 이전 패킷 데이터와 새로 받은 데이터를 합칩니다.
//    memcpy(g_recv_buffer + g_packet_offset, packet, bytes_received);
//    g_packet_offset += bytes_received;
//
//    int processed_bytes = 0;
//    while (g_packet_offset >= 1) { // 최소한 패킷 크기를 읽을 수 있는 1바이트가 있어야 함
//        unsigned char packet_size = static_cast<unsigned char>(g_recv_buffer[processed_bytes]);
//
//        if (packet_size == 0 || packet_size > BUF_SIZE) { // Prevent malformed packets causing issues
//            append_chat_message(L"경고: 잘못된 패킷 크기 수신.");
//            g_packet_offset = 0; // Reset buffer
//            processed_bytes = 0;
//            break;
//        }
//
//        if (g_packet_offset >= packet_size) { // 완전한 패킷이 도착했는지 확인
//            // 완전한 패킷이므로 처리합니다.
//            process_packet(g_recv_buffer + processed_bytes);
//
//            g_packet_offset -= packet_size;
//            processed_bytes += packet_size;
//        }
//        else {
//            // 불완전한 패킷이므로 다음 수신을 기다립니다.
//            break;
//        }
//    }
//
//    // 처리되지 않고 남은 데이터를 버퍼의 시작 부분으로 옮깁니다.
//    if (g_packet_offset > 0) {
//        memmove(g_recv_buffer, g_recv_buffer + processed_bytes, g_packet_offset);
//    }
//}
//void process_packet(char* packet)
//{
//    switch (packet[1]) {
//    case S2C_P_AVATAR_INFO: {
//        sc_packet_avatar_info* p = reinterpret_cast<sc_packet_avatar_info*>(packet);
//        if (myid == -1)
//            myid = p->id;
//        viewport_offset_x = SCREEN_WIDTH / 2 - (p->x * BlockSize);
//        viewport_offset_y = SCREEN_HEIGHT / 2 - (p->y * BlockSize);
//        map.Setoffset(viewport_offset_x, viewport_offset_y);
//        map.createobj(p->id, p->x, p->y, 0, p->hp, p->max_hp, p->exp, p->level);
//
//        std::wstring msg = L"연결 성공! ID: " + std::to_wstring(myid) + L" (" + std::to_wstring(p->x) + L", " + std::to_wstring(p->y) + L")에 로그인됨.";
//        append_chat_message(msg.c_str());
//        break;
//    }
//    case S2C_P_MOVE: {
//        sc_packet_move* p = reinterpret_cast<sc_packet_move*>(packet);
//        if (p->id == myid) {
//            viewport_offset_x = SCREEN_WIDTH / 2 - (p->x * BlockSize);
//            viewport_offset_y = SCREEN_HEIGHT / 2 - (p->y * BlockSize);
//        }
//        map.Setoffset(viewport_offset_x, viewport_offset_y);
//        map.moveobj(p->id, p->x, p->y);
//        break;
//    }
//    case S2C_P_ENTER: {
//        sc_packet_enter* p = reinterpret_cast<sc_packet_enter*>(packet);
//        map.addobj(p->id, p->x, p->y, p->o_type);
//        break;
//    }
//    case S2C_P_LEAVE: {
//        sc_packet_leave* p = reinterpret_cast<sc_packet_leave*>(packet);
//        if (p->id == myid) {
//            myid = -1;
//            append_chat_message(L"자신이 게임에서 떠났습니다.");
//        }
//        map.deleteobj(p->id);
//        break;
//    }
//    case S2C_P_STAT_CHANGE: {
//        sc_packet_stat_change* p = reinterpret_cast<sc_packet_stat_change*>(packet);
//        if (p->hp <= 0) // HP가 0 이하면 삭제 (죽음 처리)
//            map.deleteobj(p->id);
//        else {
//            int maxhp = 100 + (p->level - 1) * 10;
//            map.stat_change(p->id, p->hp, maxhp, p->exp, p->level);
//        }
//        break;
//    }
//    case S2C_P_CHAT: {
//        sc_packet_chat* p = reinterpret_cast<sc_packet_chat*>(packet);
//
//        char* chat_message_copy = new char[MAX_CHAT_LENGTH + 64]; // +64 for sender name prefix
//
//        // Use a temporary wstring for conversion
//        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
//        std::wstring w_message = converter.from_bytes(p->message);
//
//        // Simple ID to name conversion (or use a predefined name if available in packet)
//        if (p->id != -1) {
//            swprintf_s(reinterpret_cast<wchar_t*>(chat_message_copy), (MAX_CHAT_LENGTH + 64) / sizeof(WCHAR), L"Player %lld: %s", p->id, w_message.c_str());
//        }
//        else {
//            swprintf_s(reinterpret_cast<wchar_t*>(chat_message_copy), (MAX_CHAT_LENGTH + 64) / sizeof(WCHAR), L"System: %s", w_message.c_str());
//        }
//
//        // Post the message to the UI thread (WM_USER_APPEND_CHAT will handle conversion back to WCHAR)
//        PostMessage(GetAncestor(hChatDisplay, GA_ROOT), WM_USER_APPEND_CHAT, 0, (LPARAM)chat_message_copy);
//        break;
//    }
//    default:
//        std::wcerr << L"알 수 없는 패킷 타입 수신: " << static_cast<int>(packet[1]) << std::endl;
//        break;
//    }
//}
//void recv_thread_func()
//{
//    char buf[2048];
//    while (is_connected)
//    {
//        int ret = recv(client_socket, buf, sizeof(buf), 0);
//        if (ret > 0)
//        {
//            handle_packet(buf, ret);
//            PostMessage(g_hWnd, WM_USER_UPDATE_DISPLAY, 0, 0); // Request redraw
//        }
//        else if (ret == 0) // Graceful shutdown
//        {
//            append_chat_message(L"서버와 연결이 종료되었습니다.");
//            disconnect_client();
//            break; // Exit thread loop
//        }
//        else // Error
//        {
//            int error_code = WSAGetLastError();
//            if (error_code != WSAEWOULDBLOCK && is_connected) // WSAEWOULDBLOCK is normal for non-blocking, but this is blocking socket
//            {
//                std::wstring msg = L"데이터 수신 중 오류 발생! 코드: " + std::to_wstring(error_code);
//                append_chat_message(msg.c_str());
//                disconnect_client();
//            }
//            break; // Exit thread loop on error
//        }
//    }
//}
//void disconnect_client() {
//    if (!is_connected) return;
//
//    is_connected = false;
//    myid = -1; // Reset player ID
//
//    // Ensure the receive thread is stopped and joined
//    if (recvThread.joinable()) {
//        // Shutdown the socket to unblock recv() in the thread
//        shutdown(client_socket, SD_BOTH);
//        recvThread.join();
//    }
//    if (client_socket != INVALID_SOCKET) {
//        closesocket(client_socket);
//        client_socket = INVALID_SOCKET;
//    }
//    g_packet_offset = 0; // Reset packet buffer offset
//
//    // Inform UI to redraw (cleared map)
//    PostMessage(g_hWnd, WM_USER_UPDATE_DISPLAY, 0, 0);
//}
//bool connect_to_server(const char* ip_address) {
//    disconnect_client(); // Disconnect any existing connection first
//
//    append_chat_message(L"서버 연결 시도 중...");
//
//    // Initialize Winsock if not already
//    WSADATA wsaData;
//    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
//        append_chat_message(L"WSAStartup 실패!");
//        return false;
//    }
//
//    client_socket = socket(AF_INET, SOCK_STREAM, 0);
//    if (client_socket == INVALID_SOCKET) {
//        append_chat_message(L"소켓 생성 실패!");
//        WSACleanup();
//        return false;
//    }
//
//    SOCKADDR_IN server_addr;
//    memset(&server_addr, 0, sizeof(server_addr));
//    server_addr.sin_family = AF_INET;
//    server_addr.sin_port = htons(GAME_PORT);
//
//    if (inet_pton(AF_INET, ip_address, &server_addr.sin_addr) != 1) {
//        append_chat_message(L"유효하지 않은 IP 주소입니다.");
//        closesocket(client_socket);
//        client_socket = INVALID_SOCKET;
//        WSACleanup();
//        return false;
//    }
//
//    if (connect(client_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
//        int err_code = WSAGetLastError();
//        std::wstring msg = L"서버 연결 실패! 오류 코드: " + std::to_wstring(err_code);
//        append_chat_message(msg.c_str());
//        closesocket(client_socket);
//        client_socket = INVALID_SOCKET;
//        WSACleanup();
//        return false;
//    }
//
//    is_connected = true;
//    g_packet_offset = 0; // Reset packet buffer for new connection
//
//    // Start receive thread
//    // Pass a lambda to capture g_hWnd, or use a global variable.
//    // Using a global g_hWnd for simplicity given existing structure.
//    recvThread = std::thread(recv_thread_func);
//
//    // Initial login after successful connection
//    send_login("ClientUser"); // Send a default username
//
//    append_chat_message(L"서버에 연결되었습니다. 로그인 시도 중...");
//    return true;
//}
//static void send_login(const char* name)
//{
//    if (!is_connected) {
//        append_chat_message(L"로그인 패킷을 보낼 수 없습니다: 서버에 연결되지 않았습니다.");
//        return;
//    }
//    cs_packet_login login_packet;
//    login_packet.size = sizeof(login_packet);
//    login_packet.type = C2S_P_LOGIN;
//    strcpy_s(login_packet.name, sizeof(login_packet.name), name);
//    int sent_bytes = send(client_socket, reinterpret_cast<char*>(&login_packet), sizeof(login_packet), 0);
//    if (sent_bytes == SOCKET_ERROR) {
//        std::wstring msg = L"로그인 패킷 전송 실패! 오류 코드: " + std::to_wstring(WSAGetLastError());
//        append_chat_message(msg.c_str());
//        disconnect_client();
//    }
//}
//static void send_move(const char dir)
//{
//    if (!is_connected || myid == -1) {
//        append_chat_message(L"이동 패킷을 보낼 수 없습니다: 로그인되지 않았거나 연결되지 않았습니다.");
//        return;
//    }
//    cs_packet_move move_packet;
//    move_packet.size = sizeof(move_packet);
//    move_packet.type = C2S_P_MOVE;
//    move_packet.direction = dir;
//    int sent_bytes = send(client_socket, reinterpret_cast<char*>(&move_packet), sizeof(move_packet), 0);
//    if (sent_bytes == SOCKET_ERROR) {
//        std::wstring msg = L"이동 패킷 전송 실패! 오류 코드: " + std::to_wstring(WSAGetLastError());
//        append_chat_message(msg.c_str());
//        disconnect_client();
//    }
//}
//void send_attack() {
//    if (!is_connected || myid == -1) {
//        append_chat_message(L"공격 패킷을 보낼 수 없습니다: 로그인되지 않았거나 연결되지 않았습니다.");
//        return;
//    }
//    cs_packet_attack attack_packet;
//    attack_packet.size = sizeof(attack_packet);
//    attack_packet.type = C2S_P_ATTACK;
//    int sent_bytes = send(client_socket, reinterpret_cast<char*>(&attack_packet), sizeof(attack_packet), 0);
//    if (sent_bytes == SOCKET_ERROR) {
//        std::wstring msg = L"공격 패킷 전송 실패! 오류 코드: " + std::to_wstring(WSAGetLastError());
//        append_chat_message(msg.c_str());
//        disconnect_client();
//    }
//}
//void send_chat(const char* message)
//{
//    if (!is_connected || myid == -1) {
//        append_chat_message(L"채팅 패킷을 보낼 수 없습니다: 로그인되지 않았거나 연결되지 않았습니다.");
//        return;
//    }
//    cs_packet_chat chat_packet;
//    chat_packet.size = sizeof(chat_packet);
//    chat_packet.type = C2S_P_CHAT;
//    strcpy_s(chat_packet.message, MAX_CHAT_LENGTH, message);
//    int sent_bytes = send(client_socket, reinterpret_cast<char*>(&chat_packet), sizeof(chat_packet), 0);
//    if (sent_bytes == SOCKET_ERROR) {
//        std::wstring msg = L"채팅 패킷 전송 실패! 오류 코드: " + std::to_wstring(WSAGetLastError());
//        append_chat_message(msg.c_str());
//        disconnect_client();
//    }
//}
//void send_power() {
//    if (!is_connected || myid == -1) {
//        append_chat_message(L"치트 패킷을 보낼 수 없습니다: 로그인되지 않았거나 연결되지 않았습니다.");
//        return;
//    }
//    cs_packet_showmethemoney showmethemoney_packet;
//    showmethemoney_packet.size = sizeof(showmethemoney_packet);
//    showmethemoney_packet.type = C2S_P_SHOWMETHEMONEY;
//    int sent_bytes = send(client_socket, reinterpret_cast<char*>(&showmethemoney_packet), sizeof(showmethemoney_packet), 0);
//    if (sent_bytes == SOCKET_ERROR) {
//        std::wstring msg = L"치트 패킷 전송 실패! 오류 코드: " + std::to_wstring(WSAGetLastError());
//        append_chat_message(msg.c_str());
//        disconnect_client();
//    }
//}
//
//// 윈도우 프로시저 (메시지 처리 함수)
//// 윈도우 프로시저 (메시지 처리 함수)
//LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
//{
//    PAINTSTRUCT ps;
//    HDC hdc_local;
//
//    switch (uMsg)
//    {
//    case WM_CREATE: {
//        g_hWnd = hWnd; // Store global HWND
//        // 윈도우 생성 시 초기화 작업 및 컨트롤 생성
//        hChatDisplay = CreateWindowEx(
//            WS_EX_CLIENTEDGE,
//            L"EDIT",
//            L"",
//            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
//            10, SCREEN_HEIGHT - 200, SCREEN_WIDTH - 40, 100,
//            hWnd,
//            NULL,
//            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
//            NULL);
//
//        hChatEdit = CreateWindowEx(
//            WS_EX_CLIENTEDGE,
//            L"EDIT",
//            L"",
//            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
//            10, SCREEN_HEIGHT - 80, SCREEN_WIDTH - 130, 30,
//            hWnd,
//            (HMENU)1001,
//            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
//            NULL);
//
//        CreateWindow(
//            L"BUTTON",
//            L"전송",
//            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
//            SCREEN_WIDTH - 110, SCREEN_HEIGHT - 80, 90, 30,
//            hWnd,
//            (HMENU)1002,
//            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
//            NULL);
//
//        // 폰트 설정
//        HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
//            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Malgun Gothic");
//        SendMessage(hChatDisplay, WM_SETFONT, (WPARAM)hFont, TRUE);
//        SendMessage(hChatEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
//
//        append_chat_message(L"게임에 오신 것을 환영합니다! 서버에 접속하려면 /con <IP주소>를 입력하세요. 예: /con 127.0.0.1");
//        break;
//    }
//
//                  // --- WM_KEYDOWN 메시지 처리 수정 ---
//    case WM_KEYDOWN: {
//        // 채팅 입력창에 포커스가 있을 때의 처리
//        if (GetFocus() == hChatEdit) {
//            if (wParam == VK_RETURN) { // 엔터 키: 메시지 전송 버튼 클릭 효과
//                // WM_COMMAND 메시지를 수동으로 전송하여 버튼 클릭 이벤트를 트리거
//                SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(1002, BN_CLICKED), (LPARAM)GetDlgItem(hWnd, 1002));
//            }
//            else if (wParam == VK_ESCAPE) { // ESC 키: 채팅창 포커스 해제
//                SetFocus(hWnd); // 메인 윈도우에 포커스 설정
//                append_chat_message(L"채팅 입력 모드에서 벗어났습니다.");
//            }
//            // 채팅 입력창에 포커스가 있으면 다른 키 입력을 더 이상 처리하지 않음
//            break;
//        }
//
//        // 채팅 입력창에 포커스가 없을 때의 게임 조작 키 처리
//        if (is_connected && myid != -1) { // 연결되었고 로그인된 상태에서만 이동/공격 허용
//            switch (wParam) {
//            case VK_UP: send_move(MOVE_UP); break;
//            case VK_DOWN: send_move(MOVE_DOWN); break;
//            case VK_LEFT: send_move(MOVE_LEFT); break;
//            case VK_RIGHT: send_move(MOVE_RIGHT); break;
//            case 'A': send_attack(); break;
//            case 'P': send_power(); break;
//            }
//        }
//        // 게임 종료 키 (항상 작동)
//        if (wParam == 'Q') {
//            PostQuitMessage(0);
//            return 0; // 메시지 루프 종료
//        }
//        break; // WM_KEYDOWN 처리 종료
//    }
//                   // --- WM_LBUTTONDOWN 메시지 처리 추가 ---
//    case WM_LBUTTONDOWN:
//    {
//        // 현재 포커스가 hChatEdit에 있을 때만 처리
//        if (GetFocus() == hChatEdit) {
//            POINT pt;
//            pt.x = (lParam); // 클릭된 지점의 X 좌표 (클라이언트 영역 기준)
//            pt.y = (lParam); // 클릭된 지점의 Y 좌표 (클라이언트 영역 기준)
//
//            // 클릭된 지점이 hChatEdit이나 전송 버튼의 클라이언트 영역 안에 있는지 확인
//            RECT rectChatEdit, rectSendButton;
//            GetWindowRect(hChatEdit, &rectChatEdit);
//            GetWindowRect(GetDlgItem(hWnd, 1002), &rectSendButton);
//
//            // 화면 좌표로 변환된 클릭 지점 (PtInRect는 화면 좌표 기준)
//            ScreenToClient(hWnd, (LPPOINT)&pt); // 클릭 지점을 메인 윈도우의 클라이언트 좌표로 변환
//
//            // 메인 윈도우의 클라이언트 좌표에서 hChatEdit 및 전송 버튼의 클라이언트 좌표 얻기
//            RECT chatEditClient, sendButtonClient;
//            CopyRect(&chatEditClient, &rectChatEdit);
//            CopyRect(&sendButtonClient, &rectSendButton);
//            MapWindowPoints(NULL, hWnd, (LPPOINT)&chatEditClient, 2); // 화면 좌표 -> 메인 윈도우 클라이언트 좌표
//            MapWindowPoints(NULL, hWnd, (LPPOINT)&sendButtonClient, 2); // 화면 좌표 -> 메인 윈도우 클라이언트 좌표
//
//            if (!PtInRect(&chatEditClient, pt) && !PtInRect(&sendButtonClient, pt)) {
//                SetFocus(hWnd); // 메인 윈도우에 포커스 설정
//                append_chat_message(L"채팅 입력 모드에서 벗어났습니다.");
//            }
//        }
//        break;
//    }
//    case WM_PAINT: {
//        hdc_local = BeginPaint(hWnd, &ps);
//        map.Draw(hdc_local);
//        EndPaint(hWnd, &ps);
//        break;
//    }
//    case WM_USER_UPDATE_DISPLAY: {
//        InvalidateRect(hWnd, NULL, TRUE);
//        break;
//    }
//    case WM_USER_APPEND_CHAT: {
//        char* chat_message_utf8_ptr = reinterpret_cast<char*>(lParam);
//        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
//        std::wstring w_message = converter.from_bytes(chat_message_utf8_ptr);
//
//        append_chat_message(w_message.c_str()); // Use helper for consistent display
//
//        delete[] chat_message_utf8_ptr; // Free memory allocated in process_packet
//        break;
//    }
//    case WM_COMMAND: {
//        int wmId = LOWORD(wParam);
//        int wmEvent = HIWORD(wParam);
//        switch (wmId) {
//        case 1002: // Send button ID
//            if (wmEvent == BN_CLICKED) {
//                WCHAR szChatInputW[MAX_CHAT_LENGTH];
//                int cchText = GetWindowText(hChatEdit, szChatInputW, ARRAYSIZE(szChatInputW));
//
//                if (cchText > 0) {
//                    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
//                    std::string szChatInputUtf8 = converter.to_bytes(szChatInputW);
//
//                    // Check for connect command
//                    if (szChatInputUtf8.rfind("/con ", 0) == 0) { // Starts with "/connect "
//                        std::string ip_str = szChatInputUtf8.substr(strlen("/con "));
//                        append_chat_message((L"Connecting to " + converter.from_bytes(ip_str) + L"...").c_str());
//                        connect_to_server(ip_str.c_str());
//                    }
//                    else if (szChatInputUtf8.rfind("/discon", 0) == 0) { // Disconnect command
//                        append_chat_message(L"연결 해제 중...");
//                        disconnect_client();
//                        append_chat_message(L"연결이 해제되었습니다.");
//                    }
//                    else { // Regular chat message
//                        append_chat_message((L"나: " + std::wstring(szChatInputW)).c_str()); // Display own message
//                        send_chat(szChatInputUtf8.c_str());
//                    }
//
//                    SetWindowText(hChatEdit, L""); // Clear input
//                    SetFocus(hChatEdit); // Re-focus
//                }
//            }
//            break;
//        }
//        return 0;
//    }
//    case WM_DESTROY: {
//        disconnect_client(); // Ensure proper shutdown on window close
//        PostQuitMessage(0);
//        break;
//    }
//    default:
//        return DefWindowProc(hWnd, uMsg, wParam, lParam);
//    }
//    return 0;
//}
//
//LPCTSTR IpszClass = L"Window iocp";
//LPCTSTR IpszWindowName = L"window/iocp client";
//
//int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
//    MSG Message;
//    WNDCLASSEX WndClass;
//
//    // No initial IP dialog or connection attempt, rely on user command
//
//    WndClass.cbSize = sizeof(WndClass);
//    WndClass.style = CS_HREDRAW | CS_VREDRAW;
//    WndClass.lpfnWndProc = (WNDPROC)WndProc;
//    WndClass.cbClsExtra = 0;
//    WndClass.cbWndExtra = 0;
//    WndClass.hInstance = hInstance;
//    WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
//    WndClass.hCursor = LoadCursor(NULL, IDC_HAND);
//    WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
//    WndClass.lpszMenuName = NULL;
//    WndClass.lpszClassName = IpszClass;
//    WndClass.hIconSm = LoadIcon(NULL, IDI_QUESTION);
//    RegisterClassEx(&WndClass);
//
//    g_hWnd = CreateWindow(IpszClass, IpszWindowName, WS_OVERLAPPEDWINDOW | WS_SYSMENU, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, NULL, (HMENU)NULL, hInstance, NULL);
//    ShowWindow(g_hWnd, nCmdShow);
//    UpdateWindow(g_hWnd);
//
//    // Initial Winsock startup for potential future connections
//    WSADATA wsaData;
//    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
//        MessageBox(NULL, L"WSAStartup failed!", L"Error", MB_ICONERROR);
//        return -1;
//    }
//
//    // Message loop
//    while (GetMessage(&Message, 0, 0, 0))
//    {
//        TranslateMessage(&Message);
//        DispatchMessage(&Message);
//    }
//
//    // Cleanup on application exit
//    WSACleanup();
//    return Message.wParam;
//}