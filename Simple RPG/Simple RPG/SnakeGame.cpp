#pragma once
#include "Game.h"
#include "resource.h"
#pragma comment(lib, "winmm.lib")
const int WINDOW_WIDTH = 1024;
const int WINDOW_HEIGHT = 768;

PAINTSTRUCT ps;
HDC hdc;
Game g_game;
HDC g_hMemDC = NULL;
HBITMAP g_hBitmap = NULL;
HBITMAP g_hOldBitmap = NULL;

// 헬퍼 함수: Edit Control에서 텍스트를 가져와 정수로 변환하고 0~255 범위 확인
int GetIntFromEdit(HWND hDlg, int nIDDlgItem) {
    WCHAR szText[5]; // 최대 "255\0"
    GetDlgItemText(hDlg, nIDDlgItem, szText, 5);
    int value = _wtoi(szText);
    if (value < 0 || value > 255) {
        return -1; // 유효하지 않은 값
    }
    return value;
}

INT_PTR CALLBACK StartDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {

    // 게임 설명 텍스트
    const wchar_t* descriptionText =
        L"게임 설명\n"
        L"------------------------------------------\n"
        L"마우스: 이동\n"
        L"Q 키: 게임 종료\n"
        L"게임 종료 조건: 머리가 타인의 몸통에 부딪힐 경우\n"
        L"------------------------------------------";

    static userdata* pSettings = g_game.GetUserdata();
    switch (message) {
    case WM_INITDIALOG: {
        // DialogBoxParam으로 전달된 lParam을 GameSettings 구조체 포인터에 저장
        pSettings = (userdata*)lParam;

        // RGB 입력란에 기본값 설정 (예: 255, 255, 255)
        SetDlgItemText(hDlg, IDC_R_EDIT, L"255");
        SetDlgItemText(hDlg, IDC_G_EDIT, L"255");
        SetDlgItemText(hDlg, IDC_B_EDIT, L"255");

        SetDlgItemText(hDlg, IDC_DESCRIPTION_STATIC, descriptionText);

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
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDOK: {
            if (!pSettings) break;

            // 1. 닉네임 가져오기 및 확인
            GetDlgItemText(hDlg, IDC_NICKNAME_EDIT, pSettings->name, MAX_NAME_SIZE);
            if (pSettings->name[0] == L'\0') {
                MessageBox(hDlg, L"닉네임을 입력해주세요.", L"입력 오류", MB_ICONWARNING);
                SetFocus(GetDlgItem(hDlg, IDC_NICKNAME_EDIT));
                return TRUE;
            }

            // 2. RGB 값 가져오기 및 확인 (0~255 범위 체크)
            int r = GetIntFromEdit(hDlg, IDC_R_EDIT);
            int g = GetIntFromEdit(hDlg, IDC_G_EDIT);
            int b = GetIntFromEdit(hDlg, IDC_B_EDIT);

            if (r == -1 || g == -1 || b == -1) {
                MessageBox(hDlg, L"RGB 값은 0에서 255 사이의 정수여야 합니다.", L"입력 오류", MB_ICONWARNING);
                // 오류가 발생한 컨트롤에 포커스 설정 (필요시)
                return TRUE;
            }

            // 3. GameSettings에 저장 및 다이얼로그 종료
            pSettings->color = RGB(r,g,b);
            // 로그인 정보 서버로 보내기
            g_game.SendLogin();

            EndDialog(hDlg, IDOK); // IDOK로 종료
            return TRUE;
        }
        case IDCANCEL: {
            EndDialog(hDlg, IDCANCEL); // IDCANCEL로 종료
            return FALSE;
        }
        }
        break;
    }
    }
    return FALSE;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) 
    {
    case WM_CREATE: 
    {
        //g_game.Init(hdc);

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

        BitBlt(hdc,0, 0,WINDOW_WIDTH, WINDOW_HEIGHT,g_hMemDC,0, 0,SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_KEYDOWN:
    case WM_MOUSEMOVE:
        g_game.Input(wParam, lParam);
        InvalidateRect(hwnd, NULL, false);
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

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

    // 서버와 연결
    if (!g_game.InitNetwork())
        return 0;

    while (!g_game.GetLogin()) {
        // 다이얼로그 박스 표시 및 설정값 입력
        INT_PTR dialogResult = DialogBoxParam(
            hInstance,
            MAKEINTRESOURCE(IDD_START_DIALOG),
            NULL,
            StartDialogProc,
            (LPARAM)g_game.GetUserdata()
            );

        // IDOK가 아니면 (IDCANCEL 또는 오류) 프로그램 종료
        if (dialogResult != IDOK) {
            return 0;
        }
       std::this_thread::sleep_for(1ms);
        if (!g_game.GetConnect()) {
            MessageBox(NULL, L"서버에 연결되지 않았습니다. 잠시 후 재시작 합니다.", L"Error", MB_ICONERROR);
            g_game.InitNetwork();
     
            std::this_thread::sleep_for(1ms);
        }
        else if(!g_game.GetLogin())
            MessageBox(NULL, L"이미 같은 이름이 존재합니다.", L"Error", MB_ICONERROR);
    }

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"SnakeGame";

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
    HWND hwnd = CreateWindow(L"SnakeGame", L"SnakeGame",
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
            // 메시지가 없을 때 (Idle Time)
            // 별도의 스레드에서 recv 중
           
            //g_game.Update();
            
            if (g_game.GetGameover()) {
                wchar_t message_buffer[256];
                swprintf_s(message_buffer,
                    256,
                    L"당신은 죽었습니다!! 다시하시겠습니까?\nYOUR SCORE : %d", g_game.GetUserdata()->score);
                int result = MessageBox(hwnd,
                    message_buffer,
                    L"Game Over",
                    MB_YESNO | MB_ICONQUESTION);
                if (result == IDYES) {
                    // '예'를 선택한 경우: 
                    g_game.ReStart();
                    continue;
                }
                else {
                    // 사용자가 '아니오' (종료)을 눌렀을 때 
                    PostQuitMessage(0);        // 게임 종료
                    break;
                }
                continue;
            }
            
            // 업데이트 후 화면 무효화.
            InvalidateRect(hwnd, NULL, false);
        }
    }

    g_game.EndNetwork();
    g_game.StopBGM();
    // WM_QUIT 메시지의 wParam 값을 반환
    return (int)msg.wParam;
}


