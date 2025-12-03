#pragma once
constexpr unsigned short PieceSize = 30;

#include <wtypes.h>
class Object
{
public:
    long long  _id;
private:
    short _x, _y;
    char _type; // 1 이면 player, 2이면 NPC
    char _dir = 1;
    int _hp;
    int _max_hp;
    int _exp;
    int _level;
    
    //아직 안쓰는 인자들
    char name[20];
    
public:
    Object() {};
    Object(long long id, short x, short y, char type, int hp, int maxhp, int exp, int level);
    Object(long long id, short x, short y, char type);
    void move(short px, short py, char dir);
    void draw(HDC hdc, int offsetX, int offsetY);

    short GetX() const { return _x; }
    short GetY() const { return _y; }
    int GetLevel() const { return _level; }
    int GetExp() const { return _exp; }
    int GetMaxHp() const { return _max_hp; }
    int GetHp() const { return _hp; }

    void SetX(short newX) { _x = newX; }
    void SetY(short newY) { _y = newY; }
    void SetDir(char dir) { _dir = dir; }
    void SetHp(int hp) { _hp = hp; }
    void SetMaxHp(int maxhp) { _max_hp = maxhp; }
    void SetExp(int exp) { _exp = exp; }
    void SetLevel(int level) { _level = level; }
private:
    void drawperson(HDC hdc, int centerX, int centerY);
    void drawslime(HDC hdc, int centerX, int centerY);
    void drawhpbar(HDC hdc, int centerX, int centerY); // 추가: 체력바 그리기 함수
};

