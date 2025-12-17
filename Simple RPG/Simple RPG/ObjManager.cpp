#include "ObjManager.h"
constexpr auto MAP_SIZE = 700;

std::unordered_map<unsigned long long, Object> ObjManager::m_foods;
std::unordered_map<unsigned long long, Snake> ObjManager::m_snakes;

void ObjManager::AddFood(unsigned long long id, float  x, float  y, COLORREF color)
{
    Object o(x, y, color);
    m_foods.try_emplace(id, o);
}
void ObjManager::AddSnake(unsigned long long id, wchar_t* name, float  x, float  y, COLORREF color)
{
    Snake s(name, x, y, color);
    if(m_snakes.contains(id)==false)
        m_snakes.insert(std::make_pair(id, s));
}

void ObjManager::DeleteSnake(unsigned long long id)
{
    m_snakes[id].m_head.SetAlive(false);
    m_snakes[id].m_body[0].SetAlive(false);
    m_snakes.erase(id);
}
void ObjManager::DeleteFood(unsigned long long id)
{
    m_foods.erase(id);
}