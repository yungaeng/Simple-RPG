#include "World.h"

void World::Init()
{
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

void World::Draw(HDC hdc)
{
#define RGB_LIGHT_BROWN RGB(210, 180, 140) // 연갈색
#define RGB_LIME_GREEN RGB(35, 220, 65)   // 초록색
#define RGB_GRAY RGB(150, 150, 150)       // 회색

    // 사용할 브러시를 미리 생성
    HBRUSH grayBrush = CreateSolidBrush(RGB_GRAY);
    HBRUSH limeGreenBrush = CreateSolidBrush(RGB_LIME_GREEN);
    HBRUSH lightBrownBrush = CreateSolidBrush(RGB_LIGHT_BROWN);
    HBRUSH blackLineBrush = CreateSolidBrush(RGB(0, 0, 0));

    // 화면에 보이는 맵의 시작/끝 타일 좌표 계산
    // 화면에 보이는 곳(WINDOW_WIDTH와 WINDOW_HEIGHT 내에 들어오는 타일만)만 그리는 핵심 로직
    int startCol = max(0, (-offsetX) / BlockSize);
    int endCol = min(MAP_WIDTH - 1, (WINDOW_WIDTH - offsetX) / BlockSize);
    int startRow = max(0, (-offsetY) / BlockSize);
    int endRow = min(MAP_HEIGHT - 1, (WINDOW_HEIGHT - offsetY) / BlockSize);

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
    //float lineThickness = 0.f;

    //// 수직선 그리기
    //for (int col = startCol; col <= endCol; col++) {
    //    RECT lineRect = {
    //        offsetX + col * BlockSize - lineThickness / 2,
    //        offsetY + startRow * BlockSize,
    //        offsetX + col * BlockSize + lineThickness / 2,
    //        offsetY + (endRow + 1) * BlockSize
    //    };
    //    FillRect(hdc, &lineRect, blackLineBrush);
    //}

    //// 수평선 그리기
    //for (int row = startRow; row <= endRow; row++) {
    //    RECT lineRect = {
    //        offsetX + startCol * BlockSize,
    //        offsetY + row * BlockSize - lineThickness / 2,
    //        offsetX + (endCol + 1) * BlockSize,
    //        offsetY + row * BlockSize + lineThickness / 2
    //    };
    //    FillRect(hdc, &lineRect, blackLineBrush);
    //}

    //DeleteObject(blackLineBrush);
}

void World::End()
{

}

void World::UpdateOffset(float objX, float objY)
{
    // 오브젝트가 화면 중앙에 오도록 오프셋 설정
    // 공식: -(오브젝트 좌표) + (화면 크기의 절반)
    offsetX = -objX + (WINDOW_WIDTH / 2);
    offsetY = -objY + (WINDOW_HEIGHT / 2);

    // [선택 사항] 맵의 경계를 벗어나지 않게 제한하고 싶다면 아래 코드 추가
    float maxOffsetX = 0;
    float minOffsetX = -(MAP_WIDTH * BlockSize - WINDOW_WIDTH);
    float maxOffsetY = 0;
    float minOffsetY = -(MAP_HEIGHT * BlockSize - WINDOW_HEIGHT);

    if (offsetX > maxOffsetX) offsetX = maxOffsetX;
    if (offsetX < minOffsetX) offsetX = minOffsetX;
    if (offsetY > maxOffsetY) offsetY = maxOffsetY;
    if (offsetY < minOffsetY) offsetY = minOffsetY;
}