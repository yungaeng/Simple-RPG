#pragma once
#include "Game.h"
#include "resource.h"
#include <windows.h>
#include <string>

PAINTSTRUCT ps;
HDC hdc;
Game g_game;
HDC g_hMemDC = NULL;
HBITMAP g_hBitmap = NULL;
HBITMAP g_hOldBitmap = NULL;

// 직업 목록
const wchar_t* Jobs[] = { L"전사", L"마법사", L"궁수", L"도적", L"힐러" };

// Procedure 1: 로그인/회원가입 창
INT_PTR CALLBACK LoginProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG: {
        RECT rcDlg;
        // 1. 현재 다이얼로그 창의 크기를 얻습니다. (화면 좌표 기준)
        GetWindowRect(hDlg, &rcDlg);

        // 2. 다이얼로그의 폭과 높이를 계산합니다.
        int nWidth = rcDlg.right - rcDlg.left;
        int nHeight = rcDlg.bottom - rcDlg.top;

        // 3. 주 모니터의 화면 해상도를 얻습니다.
        int nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
        int nScreenHeight = GetSystemMetrics(SM_CYSCREEN);

        // 4. 다이얼로그가 화면 중앙에 위치할 좌표를 계산합니다.
        int nX = (nScreenWidth - nWidth) / 2;
        int nY = (nScreenHeight - nHeight) / 2;

        // 5. 다이얼로그 창의 위치를 설정합니다.
        SetWindowPos(hDlg,
            HWND_TOP,
            nX,
            nY,
            0, 0,
            SWP_NOSIZE | SWP_NOZORDER);

        // 포커스를 설정하지 않으면 TRUE 반환, 포커스를 직접 설정하면 FALSE 반환
        return TRUE;
    }
    case WM_COMMAND:
        // [확인/로그인] 또는 [회원가입] 버튼 클릭 시
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDC_BTN_REGISTER) {
            wchar_t id[256], pw[256];
            GetDlgItemText(hDlg, IDC_EDIT_ID, id, 256);
            GetDlgItemText(hDlg, IDC_EDIT_PW, pw, 256);

            // 1. 공통 체크: ID와 PW가 입력되었는가?
            if (wcslen(id) == 0 || wcslen(pw) == 0) {
                MessageBox(hDlg, L"ID와 PW를 입력해주세요.", L"알림", MB_OK);
                return (INT_PTR)TRUE;
            }

            // 2. 버튼 종류에 따른 분기 처리
            if (LOWORD(wParam) == IDOK) {
                // [확인/로그인] 버튼을 누른 경우:
                // 실제로는 여기서 DB나 파일을 조회해야 합니다. 
                // 지금은 예시로 "admin"이라는 아이디일 때 기존 캐릭터가 있다고 가정합니다.
                if (wcscmp(id, L"admin") == 0) {
                    // 기존 캐릭터 정보를 불러와서 세팅 (이름: 관리자, 직업: 전사)
                    g_game.SetCharacterInfo(L"관리자", 0);
                    MessageBox(hDlg, L"기존 캐릭터 정보를 불러왔습니다.", L"로그인 성공", MB_OK);
                }
                else {
                    MessageBox(hDlg, L"등록된 정보가 없습니다. 회원가입을 먼저 해주세요.", L"알림", MB_OK);
                    return (INT_PTR)TRUE;
                }
            }
            else if (LOWORD(wParam) == IDC_BTN_REGISTER) {
                // [회원가입] 버튼을 누른 경우:
                // 신규 유저이므로 g_game의 player 정보는 비어있는 상태로 유지됩니다.
                MessageBox(hDlg, L"회원가입이 완료되었습니다. 캐릭터 생성을 시작합니다.", L"알림", MB_OK);
            }

            // 창을 닫고 WinMain으로 제어권을 넘김
            EndDialog(hDlg, IDOK);
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// Procedure 2: 캐릭터 생성 창
INT_PTR CALLBACK CharCreateProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG: {
        RECT rcDlg;
        // 1. 현재 다이얼로그 창의 크기를 얻습니다. (화면 좌표 기준)
        GetWindowRect(hDlg, &rcDlg);

        // 2. 다이얼로그의 폭과 높이를 계산합니다.
        int nWidth = rcDlg.right - rcDlg.left;
        int nHeight = rcDlg.bottom - rcDlg.top;

        // 3. 주 모니터의 화면 해상도를 얻습니다.
        int nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
        int nScreenHeight = GetSystemMetrics(SM_CYSCREEN);

        // 4. 다이얼로그가 화면 중앙에 위치할 좌표를 계산합니다.
        int nX = (nScreenWidth - nWidth) / 2;
        int nY = (nScreenHeight - nHeight) / 2;

        // 5. 다이얼로그 창의 위치를 설정합니다.
        SetWindowPos(hDlg,
            HWND_TOP,
            nX,
            nY,
            0, 0,
            SWP_NOSIZE | SWP_NOZORDER);
        for (int i = 0; i < 5; i++) {
            SendDlgItemMessage(hDlg, IDC_COMBO_JOB, CB_ADDSTRING, 0, (LPARAM)Jobs[i]);
        }
        // 기본적으로 첫 번째 직업 선택
        SendDlgItemMessage(hDlg, IDC_COMBO_JOB, CB_SETCURSEL, 0, 0);
        return (INT_PTR)TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            wchar_t name[32];
            GetDlgItemText(hDlg, IDC_EDIT_NAME, name, 32); // 이름 가져오기

            int jobIndex = (int)SendDlgItemMessage(hDlg, IDC_COMBO_JOB, CB_GETCURSEL, 0, 0);
            const wchar_t* selectedJob = Jobs[jobIndex]; // 직업 가져오기

            if (wcslen(name) > 0) {
                g_game.SetCharacterInfo(name, jobIndex);
                MessageBox(hDlg, L"캐릭터 생성이 완료되었습니다!", L"RPG", MB_OK);
                EndDialog(hDlg, IDOK);
            }
            else {
                MessageBox(hDlg, L"이름을 입력해주세요.", L"알림", MB_OK);
            }
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg)
    {
    case WM_CREATE:
    {
        g_game.Init();

        HDC hdc = GetDC(hwnd);
        g_hMemDC = CreateCompatibleDC(hdc);
        g_hBitmap = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT);
        g_hOldBitmap = (HBITMAP)SelectObject(g_hMemDC, g_hBitmap);
        ReleaseDC(hwnd, hdc);
        return 0;
    }
    case WM_PAINT:
    {
        hdc = BeginPaint(hwnd, &ps);
        RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
        FillRect(g_hMemDC, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

        g_game.Draw(g_hMemDC);

        BitBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, g_hMemDC, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_KEYDOWN:
    case WM_MOUSEMOVE:
        g_game.Input(wParam, lParam);
        break;
    case WM_DESTROY:
        SelectObject(g_hMemDC, g_hOldBitmap);
        DeleteObject(g_hBitmap);
        DeleteDC(g_hMemDC);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow) {
    
    // 1. 로그인 단계
    if (DialogBox(hInstance, MAKEINTRESOURCE(IDD_LOGIN), NULL, LoginProc) != IDOK) {
        return 0; // 로그인 취소 시 종료
    }

    if (g_game.GetCharacterInfo().name.empty()) {
        // 2. 캐릭터 생성 단계
        if (DialogBox(hInstance, MAKEINTRESOURCE(IDD_CHAR_CREATE), NULL, CharCreateProc) != IDOK) {
            return 0; // 생성 취소 시 종료
        }
    }

    g_game.Init();

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"SimpleRPG";

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, L"Failed to register window class!", L"Error", MB_ICONERROR);
        return -1;
    }

    // 화면 중앙 좌표 계산
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);

    int x_pos = (screen_width - WINDOW_WIDTH) / 2;
    int y_pos = (screen_height - WINDOW_HEIGHT) / 2;

    // 윈도우 생성
    HWND hwnd = CreateWindow(L"SimpleRPG", L"SimpleRPG",
        WS_OVERLAPPEDWINDOW,
        x_pos,           // X 좌표 (화면 중앙)
        y_pos,           // Y 좌표 (화면 중앙)
        WINDOW_WIDTH,    // 폭
        WINDOW_HEIGHT,   // 높이
        NULL, NULL, hInstance, NULL);
    if (!hwnd) {
        MessageBox(NULL, L"Failed to create window!", L"Error", MB_ICONERROR);
        return -1;
    }

    //g_game.StartBGM();
    ShowWindow(hwnd, nCmdShow);
    MSG msg;

    while (true) {
        // 1. 메시지가 있는지 확인하고 있으면 처리 (Non-blocking)
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            // WM_QUIT 메시지를 받으면 루프를 종료
            if (msg.message == WM_QUIT) {
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            g_game.Update();
            // 업데이트 후 화면 무효화.
            InvalidateRect(hwnd, NULL, false);
        }
    }

    g_game.End();
    // WM_QUIT 메시지의 wParam 값을 반환
    return (int)msg.wParam;
}


