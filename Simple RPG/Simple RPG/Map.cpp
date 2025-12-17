#include "Map.h"
#include <string>
#include <sstream> // C++ 표준 라이브러리에서 문자열 스트림 사용
#include <random>
static std::random_device rd;
static std::mt19937 gen(rd());
static std::uniform_int_distribution<> distrib(0, 99); // 0부터 99까지의 난수 생성 (100가지 경우의 수)


void Map::Setoffset(int x, int y)
{
     offsetX = x;
     offsetY = y;
}

//void Map::createobj(long long  id, short x, short y, char type, int hp, int maxhp, int exp, int level)
//{
//    objects.emplace(id, Object(id, x, y, type, hp, maxhp, exp, level));
//    if(myPlayerId == -1)
//        myPlayerId = id;
//}
//
//void Map::addobj(long long id, short x, short y, char type)
//{
//    objects.emplace(id, Object(id, x, y, type));
//}

void Map::deleteobj(long long id)
{
    objects.erase(id);
}

void Map::stat_change(long long id, int hp, int maxhp, int exp, int level)
{
    auto it = objects.find(id);
    if (it != objects.end()) {        
        /*it->second.SetHp(hp);
        it->second.SetMaxHp(maxhp);
        it->second.SetExp(exp);
        it->second.SetLevel(level);*/
    }
}

void Map::moveobj(long long id, short x, short y)
{
    auto it = objects.find(id);
    if (it != objects.end()) {
        short cx = it->second.GetX(); // Current X
        short cy = it->second.GetY(); // Current Y

        // 새 위치 (x, y)와 이전 위치 (cx, cy)를 비교하여 이동 방향을 계산
        char dir = 1; // 기본값: 아래 (혹은 다른 적절한 기본 방향)
        if (x > cx) {
            dir = 3; // 오른쪽
        }
        else if (x < cx) {
            dir = 2; // 왼쪽
        }
        else if (y > cy) {
            dir = 1; // 아래
        }
        else if (y < cy) {
            dir = 0; // 위
        }

        // Update the piece's coordinates
       /* it->second.SetX(x);
        it->second.SetY(y);

        it->second.SetDir(dir);*/
    }
}

void Map::chat(long long id, const char chatMsg[100]) {
    std::wstringstream ss;
    ss << L"ID " << id << L": " << ConvertCharToWString(chatMsg);
    chatMessages.push_back(ss.str());

    // Keep only the latest messages
    if (chatMessages.size() > MAX_CHAT_MESSAGES) {
        chatMessages.erase(chatMessages.begin());
    }
}

std::wstring Map::ConvertCharToWString(const char* narrowStr)
{
    if (!narrowStr) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, narrowStr, -1, NULL, 0);
    if (len == 0) return L"";
    std::vector<wchar_t> wideBuf(len);
    MultiByteToWideChar(CP_UTF8, 0, narrowStr, -1, wideBuf.data(), len);
    return std::wstring(wideBuf.data());
}

void Map::InitializeMapTiles() {
    // 난수 생성기 초기화는 여기서 한 번만 합니다.
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 99); // 0부터 99까지의 난수 (100가지 경우의 수)

    for (int row = 0; row < MAP_HEIGHT; row++) {
        for (int col = 0; col < MAP_WIDTH; col++) {
            // 10x10 구역의 인덱스 계산 (이전과 동일한 로직)
            int area_x = col / 10;
            int area_y = row / 10;
            int area_distance = max(area_x, area_y);

            TileType currentBaseTileType; // 현재 구역의 기본 타일 타입
            TileType alternateTileType;   // 랜덤하게 선택될 대체 타일 타입

            // area_distance 값에 따라 기본 색상 설정
            if (area_distance % 2 == 0) { // 짝수 영역 인덱스 (0, 2, 4...)
                currentBaseTileType = TILE_LIGHT_BROWN; // 기본은 연갈색
                alternateTileType = TILE_LIME_GREEN;    // 대체는 초록색
            }
            else { // 홀수 영역 인덱스 (1, 3, 5...)
                currentBaseTileType = TILE_LIME_GREEN;  // 기본은 초록색
                alternateTileType = TILE_LIGHT_BROWN;   // 대체는 연갈색
            }

            // 0~9 범위의 타일 (최초 10x10 구역)은 무조건 회색
            if (row < 10 && col < 10) {
                _mapTiles[row][col] = TILE_GRAY;
            }
            else {
                // 랜덤 확률로 다른 색상 섞기 (예: 10% 확률)
                if (distrib(gen) < 10) { // 10% 확률
                    _mapTiles[row][col] = alternateTileType;
                }
                else {
                    _mapTiles[row][col] = currentBaseTileType;
                }
            }
        }
    }
}

void Map::DrawMapAndObj(HDC hdc) {
#define RGB_LIGHT_BROWN RGB(210, 180, 140) // 연갈색
#define RGB_LIME_GREEN RGB(35, 220, 65)   // 초록색
#define RGB_GRAY RGB(150, 150, 150)       // 회색

    // 사용할 브러시를 미리 생성
    HBRUSH grayBrush = CreateSolidBrush(RGB_GRAY);
    HBRUSH limeGreenBrush = CreateSolidBrush(RGB_LIME_GREEN);
    HBRUSH lightBrownBrush = CreateSolidBrush(RGB_LIGHT_BROWN);
    HBRUSH blackLineBrush = CreateSolidBrush(RGB(0, 0, 0));

    // 화면에 보이는 맵의 시작/끝 타일 좌표 계산
    int startCol = max(0, (-offsetX) / BlockSize);
    int endCol = min(MAP_WIDTH - 1, (SCREEN_WIDTH - offsetX) / BlockSize);
    int startRow = max(0, (-offsetY) / BlockSize);
    int endRow = min(MAP_HEIGHT - 1, (SCREEN_HEIGHT - offsetY) / BlockSize);

    // 맵 타일 그리기 (저장된 정보 사용)
    for (int row = startRow; row <= endRow; row++) {
        for (int col = startCol; col <= endCol; col++) {
            RECT rect = {
                offsetX + col * BlockSize,
                offsetY + row * BlockSize,
                offsetX + (col + 1) * BlockSize,
                offsetY + (row + 1) * BlockSize
            };

            HBRUSH currentBrush;
            switch (_mapTiles[row][col]) { // 저장된 타일 타입에 따라 브러시 선택
            case TILE_GRAY:
                currentBrush = grayBrush;
                break;
            case TILE_LIME_GREEN:
                currentBrush = limeGreenBrush;
                break;
            case TILE_LIGHT_BROWN:
                currentBrush = lightBrownBrush;
                break;
            default:
                currentBrush = grayBrush; // 기본값 또는 오류 처리
                break;
            }
            FillRect(hdc, &rect, currentBrush);
        }
    }

    // 브러시 삭제
    DeleteObject(grayBrush);
    DeleteObject(limeGreenBrush);
    DeleteObject(lightBrownBrush);

    // 격자선 그리기 (기존 코드와 동일)
    int lineThickness = 2;

    // 수직선 그리기
    for (int col = startCol; col <= endCol; col++) {
        RECT lineRect = {
            offsetX + col * BlockSize - lineThickness / 2,
            offsetY + startRow * BlockSize,
            offsetX + col * BlockSize + lineThickness / 2,
            offsetY + (endRow + 1) * BlockSize
        };
        FillRect(hdc, &lineRect, blackLineBrush);
    }

    // 수평선 그리기
    for (int row = startRow; row <= endRow; row++) {
        RECT lineRect = {
            offsetX + startCol * BlockSize,
            offsetY + row * BlockSize - lineThickness / 2,
            offsetX + (endCol + 1) * BlockSize,
            offsetY + row * BlockSize + lineThickness / 2
        };
        FillRect(hdc, &lineRect, blackLineBrush);
    }

    DeleteObject(blackLineBrush);

    // 피스 그리기
    for (auto p : objects) {
        //if (p.second.GetX() >= startCol && p.second.GetX() <= endCol &&
        //    p.second.GetY() >= startRow && p.second.GetY() <= endRow)
        //    /*p.second.draw(hdc, offsetX, offsetY);*/
    }
}


void Map::DrawUI(HDC hdc) {
    // --- Draw UI elements (like offset text and chat) ---
    int currentY = 10;
    int lineHeight = 20; // Adjust based on your font size

    if (!objects.empty()) {
        std::wstringstream ssCoord;
        ssCoord << L"(" << objects[myPlayerId].GetX() << L", " << objects[myPlayerId].GetY() << L")";
        TextOut(hdc, 10, currentY, ssCoord.str().c_str(), ssCoord.str().length());
        currentY += lineHeight;

        /*std::wstringstream ssLevel;
        ssLevel << L"Level : " << objects[myPlayerId].GetLevel();
        TextOut(hdc, 10, currentY, ssLevel.str().c_str(), ssLevel.str().length());
        currentY += lineHeight;

        std::wstringstream ssExp;
        ssExp << L"Exp : " << objects[myPlayerId].GetExp();
        TextOut(hdc, 10, currentY, ssExp.str().c_str(), ssExp.str().length());
        currentY += lineHeight;

        std::wstringstream ssHp;
        ssHp << L"(" << objects[0].GetHp() << L"/" << objects[myPlayerId].GetMaxHp() << L")";
        TextOut(hdc, 10, currentY, ssHp.str().c_str(), ssHp.str().length());*/
    }
    else {
        std::wstring noDataText = L"(N/A)";
        TextOut(hdc, 10, currentY, noDataText.c_str(), noDataText.length());
    }

    // Draw chat messages
    int currentChatY = CHAT_START_Y;
    for (const auto& msg : chatMessages) {
        TextOut(hdc, 10, currentChatY, msg.c_str(), msg.length());
        currentChatY += CHAT_LINE_HEIGHT;
    }
};

