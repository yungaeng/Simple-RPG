#include <iostream>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_set>
#include <concurrent_priority_queue.h>
#include <concurrent_unordered_map.h>
#include <chrono>
#include "game_header.h"
#include <lua.hpp>

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "lua54.lib")
using namespace std;

constexpr int BUF_SIZE = 200;
constexpr int NAME_SIZE = 20;
constexpr int CHAT_SIZE = 100;
constexpr int VIEW_RANGE = 15;


enum EVENT_TYPE { EV_RANDOM_MOVE, EV_LUA_CALLBACK, EV_HP_REGEN };
enum COMP_TYPE { OP_ACCEPT, OP_RECV, OP_SEND, OP_NPC_MOVE, OP_AI_HELLO };
enum S_STATE { ST_FREE, ST_ALLOC, ST_INGAME };

struct TIMER_EVENT {
	int obj_id;
	chrono::system_clock::time_point wakeup_time;
	EVENT_TYPE event_id;
	int target_id;
	constexpr bool operator < (const TIMER_EVENT& L) const
	{
		return (wakeup_time > L.wakeup_time);
	}
};
concurrency::concurrent_priority_queue<TIMER_EVENT> timer_queue;

class OVER_EXP {
public:
	WSAOVERLAPPED _over;
	WSABUF _wsabuf;
	char _send_buf[BUF_SIZE];
	COMP_TYPE _comp_type;
	int _ai_target_obj;
	OVER_EXP()
	{
		_wsabuf.len = BUF_SIZE;
		_wsabuf.buf = _send_buf;
		_comp_type = OP_RECV;
		ZeroMemory(&_over, sizeof(_over));
	}
	OVER_EXP(char* packet)
	{
		_wsabuf.len = packet[0];
		_wsabuf.buf = _send_buf;
		ZeroMemory(&_over, sizeof(_over));
		_comp_type = OP_SEND;
		memcpy(_send_buf, packet, packet[0]);
	}
};

class SESSION {
	OVER_EXP _recv_over;

public:
	mutex _s_lock;
	S_STATE _state;
	atomic_bool	_is_active;		// 주위에 플레이어가 있는가?
	long long _id;
	SOCKET _socket;
	short	x, y;
	int _hp;
	int _atk;
	int _exp;
	int _level;

	char	_name[NAME_SIZE];
	int		_prev_remain;
	unordered_set <int> _view_list;
	mutex	_vl;
	int		last_move_time;
	lua_State* _L;
	mutex	_ll;
public:
	SESSION() {
		_id = -1;
		_socket = 0;
		x = y = 0;
		_hp = 50;
		_atk = 10;
		_exp = 0;
		_level = 1;
		_name[0] = 0;
		_state = ST_FREE;
		_prev_remain = 0;
		last_move_time = 0;
		_L = nullptr;
	}
	~SESSION() {}

	void do_recv() {
		DWORD recv_flag = 0;
		memset(&_recv_over._over, 0, sizeof(_recv_over._over));
		_recv_over._wsabuf.len = BUF_SIZE - _prev_remain;
		_recv_over._wsabuf.buf = _recv_over._send_buf + _prev_remain;
		WSARecv(_socket, &_recv_over._wsabuf, 1, 0, &recv_flag,
			&_recv_over._over, 0);
	}
	void do_send(void* packet)
	{
		OVER_EXP* sdata = new OVER_EXP{ reinterpret_cast<char*>(packet) };
		WSASend(_socket, &sdata->_wsabuf, 1, 0, 0, &sdata->_over, 0);
	}
	void send_login_info_packet()
	{
		sc_packet_avatar_info p;
		p.id = _id;
		p.size = sizeof(p);
		p.type = S2C_P_AVATAR_INFO;
		p.x = x;
		p.y = y;
		p.hp = _hp;
		p.max_hp = _level * 100;
		p.exp = _exp;
		p.level = _level;
		do_send(&p);
	}
	void send_login_fail_packet(char reason) {
		sc_packet_login_fail p;
		p.id = _id;
		p.size = sizeof(p);
		p.type = S2C_P_LOGIN_FAIL;
		p.reason = reason;
		do_send(&p);
	}
	void send_leave_packet(long long c_id)
	{
		_vl.lock();
		if (_view_list.count(c_id))
			_view_list.erase(c_id);
		else {
			_vl.unlock();
			return;
		}
		_vl.unlock();
		sc_packet_leave p;
		p.id = c_id;
		p.size = sizeof(p);
		p.type = S2C_P_LEAVE;
		do_send(&p);
	}
	void send_stat_change(int affected_id, int hp, int exp, int level) {
		sc_packet_stat_change p;
		p.size = sizeof(p);
		p.type = S2C_P_STAT_CHANGE;
		p.id = affected_id; // Now takes the ID of the object whose stats changed
		p.hp = hp;
		p.exp = exp;
		p.level = level;
		do_send(&p);
	}

	void send_move_packet(int c_id);
	void send_enter_packet(int c_id);
	void send_chat_packet(int c_id, const char* mess);
};
concurrency::concurrent_unordered_map<long long, std::shared_ptr<SESSION>> clients;

HANDLE h_iocp;
SOCKET g_s_socket, g_c_socket;
OVER_EXP g_a_over;

bool is_pc(int object_id) {
	return object_id < MAX_USER;
}
bool is_npc(int object_id) {
	return !is_pc(object_id);
}
bool can_see(int from, int to) {
	if (abs(clients[from]->x - clients[to]->x) > VIEW_RANGE) return false;
	return abs(clients[from]->y - clients[to]->y) <= VIEW_RANGE;
}

void SESSION::send_move_packet(int c_id) {
	sc_packet_move p;
	p.id = c_id;
	p.size = sizeof(p);
	p.type = S2C_P_MOVE;
	p.x = clients[c_id]->x;
	p.y = clients[c_id]->y;
	//p.move_time = clients[c_id]->last_move_time;
	do_send(&p);
}
void SESSION::send_enter_packet(int c_id) {
	sc_packet_enter p;
	p.id = c_id;
	strcpy_s(p.name, clients[c_id]->_name);
	p.size = sizeof(p);
	p.type = S2C_P_ENTER;
	p.x = clients[c_id]->x;
	p.y = clients[c_id]->y;
	char o_type = is_npc(c_id);	// 0이면 플레이어, 1이면 npc
	p.o_type = o_type;
	_vl.lock();
	_view_list.insert(c_id);
	_vl.unlock();
	do_send(&p);
}
void SESSION::send_chat_packet(int p_id, const char* message) {
	sc_packet_chat p;
	p.id = p_id;
	p.size = sizeof(p);
	p.type = S2C_P_CHAT;
	strcpy_s(p.message, message);
	do_send(&p);
}
long long get_new_client_id() {
	for (long long i = 0; i < MAX_USER; ++i) {
		lock_guard <mutex> ll{ clients[i]->_s_lock };
		if (clients[i]->_state == ST_FREE)
			return i;
	}
	return -1;
}

void WakeUpNPC(int npc_id, int waker)
{
	OVER_EXP* exover = new OVER_EXP;
	exover->_comp_type = OP_AI_HELLO;
	exover->_ai_target_obj = waker;
	PostQueuedCompletionStatus(h_iocp, 1, npc_id, &exover->_over);

	if (clients[npc_id]->_is_active) return;
	bool old_state = false;
	if (false == atomic_compare_exchange_strong(&clients[npc_id]->_is_active, &old_state, true))
		return;
	TIMER_EVENT ev{ npc_id, chrono::system_clock::now(), EV_RANDOM_MOVE, 0 };
	timer_queue.push(ev);
}
int GetExpToNextLevel(int current_level) {
	return current_level * 100;
}
int LevelUp(int current_exp) {
	// 현재 누적 경험치로 도달할 수 있는 레벨을 계산합니다.
	int level = 1;
	int exp_needed_for_current_level_up = 0; // 현재 레벨에서 다음 레벨로 가기 위해 필요한 누적 경험치

	while (true) {
		int exp_for_next_level = GetExpToNextLevel(level); // 현재 레벨에서 다음 레벨까지 '추가로' 필요한 경험치
		if (current_exp >= exp_needed_for_current_level_up + exp_for_next_level) {
			exp_needed_for_current_level_up += exp_for_next_level;
			level++;
		}
		else {
			break;
		}
	}
	return level;
}
void process_packet(int c_id, char* packet)
{
	switch (packet[1]) {
	case C2S_P_LOGIN: {
		cs_packet_login* p = reinterpret_cast<cs_packet_login*>(packet);
		strcpy_s(clients[c_id]->_name, p->name);
		{
			lock_guard<mutex> ll{ clients[c_id]->_s_lock };
			clients[c_id]->x = rand() % 10;
			clients[c_id]->y = rand() % 10;
			clients[c_id]->_state = ST_INGAME;
		}
		clients[c_id]->send_login_info_packet();

		// 플레이어 로그인 시 HP 회복 이벤트 등록
		TIMER_EVENT hp_regen_event{ c_id, chrono::system_clock::now() + 5s, EV_HP_REGEN, 0 };
		timer_queue.push(hp_regen_event);

		for (auto& pl : clients) {
			{
				lock_guard<mutex> ll(pl.second->_s_lock);
				if (ST_INGAME != pl.second->_state) continue;
			}
			if (pl.second->_id == c_id) continue;
			if (false == can_see(c_id, pl.second->_id))
				continue;
			if (is_pc(pl.second->_id)) pl.second->send_enter_packet(c_id);
			else WakeUpNPC(pl.second->_id, c_id);
			clients[c_id]->send_enter_packet(pl.second->_id);
		}
		break;
	}
	case C2S_P_MOVE: {
		cs_packet_move* p = reinterpret_cast<cs_packet_move*>(packet);
		//clients[c_id]->last_move_time = p->move_time;
		short current_x = clients[c_id]->x;
		short current_y = clients[c_id]->y;

		short target_x = current_x;
		short target_y = current_y;

		// Determine the target coordinates based on direction
		switch (p->direction) {
		case MOVE_UP: if (target_y > 0) target_y--; break;					// 위
		case MOVE_DOWN: if (target_y < MAP_HEIGHT - 1) target_y++; break;	// 아래
		case MOVE_LEFT: if (target_x > 0) target_x--; break;				// 왼쪽
		case MOVE_RIGHT: if (target_x < MAP_WIDTH - 1) target_x++; break;	// 오른쪽
		}

		// --- 충돌 체크 ---
		bool can_move = true;
		// 충돌 체크는 이동할 때만 계산합니다
		if (target_x != current_x || target_y != current_y) {
			// 모든 플레이어를 순회하며 충돌검사를 진행합니다
			for (auto& cl : clients) {
				// 자기 자신은 체크 안함
				if (cl.second->_id == c_id) continue;

				// 플레이어 정보를 확인하기 위한 락
				std::lock_guard<std::mutex> ll(cl.second->_s_lock);
				if (cl.second->_state == ST_INGAME && cl.second->x == target_x && cl.second->y == target_y) {
					can_move = false;
					break;
				}
			}
		}

		// 이동이 가능함
		if (can_move) {
			clients[c_id]->x = target_x;
			clients[c_id]->y = target_y;


			std::unordered_set<int> near_list;
			clients[c_id]->_vl.lock();
			std::unordered_set<int> old_vlist = clients[c_id]->_view_list;
			clients[c_id]->_vl.unlock();
			for (auto& cl : clients) {
				if (cl.second->_state != ST_INGAME) continue;
				if (cl.second->_id == c_id) continue;
				if (can_see(c_id, cl.second->_id))
					near_list.insert(cl.second->_id);
			}

			clients[c_id]->send_move_packet(c_id);

			for (auto& pl : near_list) {
				auto& cpl = clients[pl];
				if (is_pc(pl)) {
					cpl->_vl.lock();
					if (clients[pl]->_view_list.count(c_id)) {
						cpl->_vl.unlock();
						clients[pl]->send_move_packet(c_id);
					}
					else {
						cpl->_vl.unlock();
						clients[pl]->send_enter_packet(c_id);
					}
				}
				else WakeUpNPC(pl, c_id);

				if (old_vlist.count(pl) == 0)
					clients[c_id]->send_enter_packet(pl);
			}

			for (auto& pl : old_vlist)
				if (0 == near_list.count(pl)) {
					clients[c_id]->send_leave_packet(pl);
					if (is_pc(pl))
						clients[pl]->send_leave_packet(c_id);
				}
		}

		break;
	}
	case C2S_P_ATTACK: {
		cs_packet_attack* p = reinterpret_cast<cs_packet_attack*>(packet);

		// 1. 공격자 정보 획득
		// 1-1 session에서 얻을 정보
		// 위치, 공격력
		short attacker_x, attacker_y;
		int attacker_atk;
		char attacker_dir;
		// 1-2 패킷에서 얻을 정보
		// 공격자 아이디
		long long attacker_id = c_id;

		std::shared_ptr<SESSION> attacker_session = nullptr;
		{
			auto it = clients.find(attacker_id);
			if (it == clients.end()) {
				return; // 공격자 클라이언트가 이미 존재하지 않는 경우 (예: 로그아웃 등)
			}
			attacker_session = it->second;
		}

		{ // 공격자 정보를 얻어내기 위한 락
			std::lock_guard<std::mutex> attacker_lock(attacker_session->_s_lock);
			attacker_x = attacker_session->x;
			attacker_y = attacker_session->y;
			attacker_atk = attacker_session->_atk;
		}

		// 공격 대상 타일 계산
		// 4방향 공격 게산하기 위해, 체크할 좌표들이 담긴 vector 생성함
		std::vector<std::pair<short, short>> attack_targets;
		attack_targets.push_back({ attacker_x, attacker_y - 1 }); // 위
		attack_targets.push_back({ attacker_x, attacker_y + 1 }); // 아래
		attack_targets.push_back({ attacker_x - 1, attacker_y }); // 왼쪽
		attack_targets.push_back({ attacker_x + 1, attacker_y }); // 오른쪽

		// 대상 타일이 유효 범위 내에 있는지 확인
		for (auto target_pos : attack_targets) {
			if (target_pos.first < 0 || target_pos.first >= MAP_WIDTH || target_pos.second < 0 || target_pos.second >= MAP_HEIGHT) {
				std::cout << "Miss! Attacker ID: " << attacker_id << " attacked out of bounds.\n";
				break;
			}
		}

		// 2. 대상 찾기
		std::shared_ptr<SESSION> hit_target_session = nullptr;
		long long hit_target_id = -1;
		for (auto& entry : clients) {
			if (entry.second->_id == attacker_id) continue;
			{
				std::lock_guard<std::mutex> current_client_lock(entry.second->_s_lock);
				for (auto target_pos : attack_targets) {
					if (entry.second->_state == ST_INGAME &&
						entry.second->x == target_pos.first && entry.second->y == target_pos.second) {
						hit_target_session = entry.second;
						hit_target_id = entry.second->_id;
						break;
					}
				}
			}
		}

		// 3. 대상이 존재하고 타격이 성공했다면 처리
		if (hit_target_session) {
			std::lock_guard<std::mutex> target_lock(hit_target_session->_s_lock);

			short new_target_hp = hit_target_session->_hp - attacker_atk;
			if (new_target_hp < 0) new_target_hp = 0; // HP가 음수가 되지 않도록

			std::cout << "HIT! Attacker ID: " << attacker_id
				<< " hit target ID: " << hit_target_id << " New HP: " << new_target_hp << "\n";

			hit_target_session->_hp = new_target_hp; // 서버 측 대상 HP 업데이트

			// **몬스터 처치 로직 (기존과 동일)**
			if (new_target_hp == 0 && is_npc(hit_target_id)) {
				std::cout << "NPC " << hit_target_id << " defeated by Attacker " << attacker_id << "!\n";

				hit_target_session->_state = ST_FREE;
				for (auto& pl_entry : clients) {
					if (pl_entry.second->_state == ST_INGAME && is_pc(pl_entry.second->_id)) {
						pl_entry.second->send_leave_packet(hit_target_id);
					}
				}

				// 공격자 경험치 및 레벨업 처리 (기존과 동일)
				{
					std::lock_guard<std::mutex> attacker_stats_lock(attacker_session->_s_lock);
					int old_exp = attacker_session->_exp;
					int old_level = attacker_session->_level;

					attacker_session->_exp += 10;
					int new_level = LevelUp(attacker_session->_exp);
					if (new_level > old_level) {
						attacker_session->_level = new_level;
						attacker_session->_hp = attacker_session->_level * 100; // 레벨업 시 HP 최대치
						std::cout << "Attacker ID: " << attacker_id << " Leveled Up to " << attacker_session->_level << "!\n";
					}
					attacker_session->send_stat_change(attacker_id,
						attacker_session->_hp,
						attacker_session->_exp,
						attacker_session->_level);
				}
			}
			// **플레이어 HP가 0이 되었을 때의 처리 (추가된 부분)**
			else if (new_target_hp == 0 && is_pc(hit_target_id)) { // 플레이어가 사망했을 경우
				std::cout << "Player " << hit_target_id << " has died!\n";

				// 1. 경험치 절반으로 감소
				hit_target_session->_exp /= 2;
				// 경험치 감소로 인해 레벨이 내려갈 수도 있으므로 레벨 재계산 (옵션)
				hit_target_session->_level = LevelUp(hit_target_session->_exp);

				// 2. HP 회복
				hit_target_session->_hp = hit_target_session->_level * 100;

				// 3. 시작 위치 (10*10 랜덤)로 이동
				short old_x = hit_target_session->x;
				short old_y = hit_target_session->y;
				hit_target_session->x = rand() % 10; // 0-9 사이의 랜덤 X
				hit_target_session->y = rand() % 10; // 0-9 사이의 랜덤 Y

				std::cout << "Player " << hit_target_id << " moved to respawn point (" << hit_target_session->x << ", " << hit_target_session->y << ").\n";

				// 시야에 있던 다른 플레이어들에게 사망한 플레이어의 "이동" 및 "스탯 변경" 알림
				// 먼저 사라진 위치에서 leave 패킷을 보내고, 새 위치로 enter/move 패킷을 보냅니다.
				std::unordered_set<int> old_view_list;
				std::unordered_set<int> new_view_list;

				// 기존 시야에 있던 플레이어들에게 사망한 플레이어의 "이동" 알림 (사라짐)
				hit_target_session->_vl.lock();
				old_view_list = hit_target_session->_view_list;
				hit_target_session->_view_list.clear(); // 시야 목록 초기화
				hit_target_session->_vl.unlock();

				for (int other_id : old_view_list) {
					if (clients.count(other_id) && clients[other_id]->_state == ST_INGAME && is_pc(other_id)) {
						clients[other_id]->send_leave_packet(hit_target_id);
					}
				}

				// 사망한 플레이어에게 변경된 스탯 (HP, EXP, Level) 및 이동 정보 전송
				hit_target_session->send_stat_change(hit_target_id, hit_target_session->_hp, hit_target_session->_exp, hit_target_session->_level);
				hit_target_session->send_move_packet(hit_target_id);

				// 이제 새로 이동한 위치를 기준으로 주변 플레이어들에게 자신의 정보를 알리고,
				// 주변 플레이어들의 정보를 새로 업데이트
				for (auto& pl : clients) {
					if (pl.second->_state != ST_INGAME) continue;
					if (pl.second->_id == hit_target_id) continue;

					if (can_see(hit_target_id, pl.second->_id)) {
						// 사망한 플레이어 시야에 새로운 플레이어가 들어옴
						hit_target_session->send_enter_packet(pl.second->_id);
					}
					if (can_see(pl.second->_id, hit_target_id) && is_pc(pl.second->_id)) {
						// 다른 플레이어 시야에 사망한 플레이어가 새로 들어옴
						pl.second->send_enter_packet(hit_target_id);
					}
				}
			}

			// Stat Change 패킷 생성 (공격 받은 대상의 변경된 상태)
			// 이 부분은 NPC든 플레이어든 HP 변경이 있으면 항상 실행됩니다.
			sc_packet_stat_change stat_change_packet;
			stat_change_packet.size = sizeof(stat_change_packet);
			stat_change_packet.type = S2C_P_STAT_CHANGE;
			stat_change_packet.id = hit_target_id;
			stat_change_packet.hp = hit_target_session->_hp; // 이미 업데이트된 HP
			stat_change_packet.exp = hit_target_session->_exp;
			stat_change_packet.level = hit_target_session->_level;

			// 4. 관련 클라이언트들에게 상태 변경 알림 (기존과 동일, 단 사망 처리된 플레이어는 이미 위에서 처리)
			for (auto& pl_entry : clients) {
				if (pl_entry.second->_state == ST_INGAME &&
					pl_entry.second->_id != hit_target_id &&
					can_see(pl_entry.second->_id, hit_target_id))
				{
					pl_entry.second->send_stat_change(
						stat_change_packet.id, stat_change_packet.hp,
						stat_change_packet.exp, stat_change_packet.level
					);
				}
			}
			// 공격자 본인에게도 대상의 HP 변경을 알림
			if (can_see(attacker_id, hit_target_id)) {
				attacker_session->send_stat_change(
					stat_change_packet.id, stat_change_packet.hp,
					stat_change_packet.exp, stat_change_packet.level
				);
			}
		}
		break;
	}
	case C2S_P_CHAT: {
		cs_packet_chat* p = reinterpret_cast<cs_packet_chat*>(packet);
		cout << "ID : " << c_id << " Chat : " << p->message << endl;
		for (auto& pl : clients) {
			{
				lock_guard<mutex> ll(pl.second->_s_lock);
				if (ST_INGAME != pl.second->_state) continue;
			}
			if (c_id == pl.second->_id) continue;
			if (is_pc(pl.second->_id))
				clients[c_id]->send_chat_packet(c_id, p->message);
		}
		break;
	}
	case C2S_P_SHOWMETHEMONEY: {

		{
			lock_guard<mutex> ll{ clients[c_id]->_s_lock };
			clients[c_id]->_atk = 10000;
			clients[c_id]->_exp = 1000000;

		}

		// 2. 대상 찾기
		std::shared_ptr<SESSION> hit_target_session = nullptr;
		long long hit_target_id = -1;
		for (auto& entry : clients) {
			if (entry.second->_id == c_id) continue;
			{
				std::lock_guard<std::mutex> current_client_lock(entry.second->_s_lock);
				{
					hit_target_session = entry.second;
					hit_target_id = entry.second->_id;
					break;
				}
			}
		}

		// Stat Change 패킷 생성 (공격 받은 대상의 변경된 상태)
			// 이 부분은 NPC든 플레이어든 HP 변경이 있으면 항상 실행됩니다.
		sc_packet_stat_change stat_change_packet;
		stat_change_packet.size = sizeof(stat_change_packet);
		stat_change_packet.type = S2C_P_STAT_CHANGE;
		stat_change_packet.id = hit_target_id;
		stat_change_packet.hp = 100000;
		stat_change_packet.exp = hit_target_session->_exp;
		stat_change_packet.level = hit_target_session->_level;

		// 4. 관련 클라이언트들에게 상태 변경 알림 (기존과 동일, 단 사망 처리된 플레이어는 이미 위에서 처리)
		for (auto& pl_entry : clients) {
			if (pl_entry.second->_state == ST_INGAME &&
				pl_entry.second->_id != hit_target_id &&
				can_see(pl_entry.second->_id, hit_target_id))
			{
				pl_entry.second->send_stat_change(
					stat_change_packet.id, stat_change_packet.hp,
					stat_change_packet.exp, stat_change_packet.level
				);
			}
		}
		// 공격자 본인에게도 대상의 HP 변경을 알림
		if (can_see(c_id, hit_target_id)) {
			hit_target_session->send_stat_change(
				stat_change_packet.id, stat_change_packet.hp,
				stat_change_packet.exp, stat_change_packet.level
			);
		}
		break;
	}
	case C2S_P_TELEPORT: {
		short old_x, old_y;
		short new_x, new_y;

		{ // 1. 플레이어의 위치를 즉시 무작위로 변경
			lock_guard<mutex> ll{ clients[c_id]->_s_lock };
			old_x = clients[c_id]->x; // 이전 위치 저장
			old_y = clients[c_id]->y;

			// 새로운 무작위 위치 계산
			new_x = rand() % MAP_WIDTH;
			new_y = rand() % MAP_HEIGHT;

			// TODO: (옵션) 텔레포트할 새로운 위치가 다른 객체와 겹치는지 확인하는 충돌 검사 추가.
			// 현재는 텔레포트 대상 위치에 다른 객체가 있어도 그냥 이동시킵니다.
			// 만약 겹치지 않는 위치를 찾고 싶다면, 여기에서 루프를 돌며 `can_move`와 유사한 검사를 수행하고,
			// 겹치지 않는 위치를 찾을 때까지 `rand()`를 다시 호출할 수 있습니다.
			// 예:
			// bool collided;
			// do {
			//     collided = false;
			//     new_x = rand() % MAP_WIDTH;
			//     new_y = rand() % MAP_HEIGHT;
			//     for (auto& cl : clients) {
			//         if (cl.second->_id == c_id) continue;
			//         lock_guard<mutex> other_ll(cl.second->_s_lock);
			//         if (cl.second->_state == ST_INGAME && cl.second->x == new_x && cl.second->y == new_y) {
			//             collided = true;
			//             break;
			//         }
			//     }
			// } while (collided);

			clients[c_id]->x = new_x; // 새 위치로 업데이트
			clients[c_id]->y = new_y;
		} // 락 해제

		// 2. 텔레포트한 플레이어에게 자신의 새로운 위치를 전송
		clients[c_id]->send_move_packet(c_id);

		// 3. 시야 목록 갱신 및 주변 플레이어들에게 정보 전송 (기존 이동 로직 재활용)
		std::unordered_set<int> near_list; // 텔레포트 후 새로 시야에 들어온 객체
		clients[c_id]->_vl.lock();
		std::unordered_set<int> old_vlist = clients[c_id]->_view_list; // 텔레포트 전 시야에 있던 객체
		clients[c_id]->_view_list.clear(); // 시야 목록 초기화 (텔레포트이므로)
		clients[c_id]->_vl.unlock();

		// 텔레포트한 플레이어의 새로운 시야에 들어오는 객체들을 찾습니다.
		for (auto& cl : clients) {
			if (cl.second->_state != ST_INGAME) continue;
			if (cl.second->_id == c_id) continue;
			if (can_see(c_id, cl.second->_id))
				near_list.insert(cl.second->_id);
		}

		// 3-1. 텔레포트한 플레이어의 시야에 새로 들어온 객체들에게 ENTER 패킷을 보냅니다.
		for (auto& other_id : near_list) {
			// 텔레포트한 플레이어가 old_vlist에는 없었지만, now_vlist에 있는 경우 (새로 발견한 경우)
			if (old_vlist.count(other_id) == 0) {
				clients[c_id]->send_enter_packet(other_id);
			}
			else {
				// 텔레포트 전에도 보였던 객체라면 (같은 시야 범위 내에서 텔레포트), MOVE 패킷으로 업데이트
				// 이 부분은 사실상 발생하기 어렵거나 텔레포트의 경우 생략해도 될 수 있음.
				// 하지만 정확성을 위해 포함.
				clients[c_id]->send_move_packet(other_id);
			}
		}


		// 3-2. 주변 플레이어 및 NPC들에게 텔레포트한 플레이어의 위치 변경을 알립니다.
		for (auto& other_entry : clients) {
			auto& other_client = other_entry.second;
			if (other_client->_state != ST_INGAME) continue;
			if (other_client->_id == c_id) continue; // 자기 자신 제외

			// 이전에 이 플레이어를 보던 다른 플레이어가 있었고, 이제는 시야 밖으로 나갔다면 LEAVE 패킷 전송
			if (old_vlist.count(other_client->_id) && !can_see(other_client->_id, c_id)) {
				// 다른 플레이어의 시야에 c_id가 없는데 old_vlist에 있었다면 (사라짐)
				if (is_pc(other_client->_id)) {
					other_client->send_leave_packet(c_id);
				}
			}
			// 이전에 이 플레이어를 보지 못했지만, 이제 시야에 들어왔다면 ENTER 패킷 전송
			else if (!old_vlist.count(other_client->_id) && can_see(other_client->_id, c_id)) {
				// 다른 플레이어 시야에 c_id가 새로 들어왔다면 (등장)
				if (is_pc(other_client->_id)) {
					other_client->send_enter_packet(c_id);
				}
				else { // NPC라면 깨우기
					WakeUpNPC(other_client->_id, c_id);
				}
			}
			// 이전에도 보았고, 지금도 보고 있다면 MOVE 패킷 전송
			else if (old_vlist.count(other_client->_id) && can_see(other_client->_id, c_id)) {
				// 다른 플레이어 시야에 c_id가 계속 있다면 (이동)
				if (is_pc(other_client->_id)) {
					other_client->send_move_packet(c_id);
				}
				else { // NPC라면 깨우기
					WakeUpNPC(other_client->_id, c_id);
				}
			}
		}

		// 4. old_vlist에 있었지만 이제 near_list에 없는 객체들 처리
		// 이는 텔레포트한 플레이어가 자신의 시야에서 사라진 객체들에게 LEAVE 패킷을 보내는 로직입니다.
		for (auto& old_obj_id : old_vlist) {
			if (0 == near_list.count(old_obj_id)) {
				clients[c_id]->send_leave_packet(old_obj_id);
				// 만약 사라진 객체가 PC라면, 그 PC의 시야에서도 c_id가 사라졌음을 알려야 합니다.
				// (위의 3-2에서 이미 처리되므로 여기서는 생략 가능하지만, 명확성을 위해 다시 생각할 수 있음)
				if (is_pc(old_obj_id) && clients.count(old_obj_id) && clients[old_obj_id]->_state == ST_INGAME) {
					// clients[old_obj_id]->send_leave_packet(c_id); // 이미 3-2에서 처리되었을 가능성 있음
				}
			}
		}
		break;
	}
	default:
		break;
	}
}
void disconnect(int c_id) {
	clients[c_id]->_vl.lock();
	unordered_set <int> vl = clients[c_id]->_view_list;
	clients[c_id]->_vl.unlock();
	for (auto& p_id : vl) {
		if (is_npc(p_id)) continue;
		auto& pl = clients[p_id];
		{
			lock_guard<mutex> ll(pl->_s_lock);
			if (ST_INGAME != pl->_state) continue;
		}
		if (pl->_id == c_id) continue;
		pl->send_leave_packet(c_id);
	}
	closesocket(clients[c_id]->_socket);

	lock_guard<mutex> ll(clients[c_id]->_s_lock);
	clients[c_id]->_state = ST_FREE;
}
void do_npc_random_move(int npc_id) {
	std::shared_ptr<SESSION> npc = clients[npc_id];

	// Lock the NPC's state to prevent race conditions during movement calculation
	std::lock_guard<std::mutex> npc_lock(npc->_s_lock);

	unordered_set<int> old_vl;
	for (auto& obj : clients) {
		if (ST_INGAME != obj.second->_state) continue;
		if (true == is_npc(obj.second->_id)) continue;
		if (true == can_see(npc->_id, obj.second->_id))
			old_vl.insert(obj.second->_id);
	}

	int current_x = npc->x;
	int current_y = npc->y;
	int target_x = current_x;
	int target_y = current_y;
	char new_dir = rand() % 4; // Store the chosen direction

	// Determine the target coordinates based on the random direction
	switch (new_dir) {
	case 0: if (target_x < (MAP_WIDTH - 1)) target_x++; break; // MOVE_RIGHT (Assuming 0 is Right for rand()%4)
	case 1: if (target_x > 0) target_x--; break; // MOVE_LEFT (Assuming 1 is Left)
	case 2: if (target_y < (MAP_HEIGHT - 1)) target_y++; break; // MOVE_DOWN (Assuming 2 is Down)
	case 3:if (target_y > 0) target_y--; break; // MOVE_UP (Assuming 3 is Up)
	}

	// --- Collision Detection for NPC ---
	bool can_move = true;
	// Only check collision if NPC is attempting to move to a different tile
	if (target_x != current_x || target_y != current_y) {
		for (auto& cl : clients) {
			// Don't check against self
			if (cl.second->_id == npc_id) continue;

			// Acquire lock for the potential blocking client
			std::lock_guard<std::mutex> cl_lock(cl.second->_s_lock);
			if (cl.second->_state == ST_INGAME && cl.second->x == target_x && cl.second->y == target_y) {
				// Target tile is occupied by another entity (player or NPC)
				can_move = false;
				break; // Collision found, no need to check further
			}
		}
	}
	else {
		// If the NPC attempts to move off-map or stays in place, 
		// it's not a collision in the sense of an occupied tile.
		// We still need to update its direction.
	}

	if (can_move) {
		// Update NPC's position
		npc->x = target_x;
		npc->y = target_y;
	}

	// Rest of the view list update logic remains similar
	unordered_set<int> new_vl;
	for (auto& obj : clients) {
		if (ST_INGAME != obj.second->_state) continue;
		if (true == is_npc(obj.second->_id)) continue;
		if (true == can_see(npc->_id, obj.second->_id))
			new_vl.insert(obj.second->_id);
	}

	for (auto pl : new_vl) {
		if (0 == old_vl.count(pl)) {
			// 플레이어의 시야에 등장
			clients[pl]->send_enter_packet(npc->_id);
		}
		else {
			// 플레이어가 계속 보고 있음.
			clients[pl]->send_move_packet(npc->_id);
		}
	}

	for (auto pl : old_vl) {
		if (0 == new_vl.count(pl)) {
			clients[pl]->_vl.lock();
			if (0 != clients[pl]->_view_list.count(npc->_id)) {
				clients[pl]->_vl.unlock();
				clients[pl]->send_leave_packet(npc->_id);
			}
			else {
				clients[pl]->_vl.unlock();
			}
		}
	}
}
void worker_thread(HANDLE h_iocp) {
	while (true) {
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* over = nullptr;
		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
		OVER_EXP* ex_over = reinterpret_cast<OVER_EXP*>(over);
		if (FALSE == ret) {
			if (ex_over->_comp_type == OP_ACCEPT) cout << "Accept Error";
			else {
				cout << "GQCS Error on client[" << key << "]\n";
				disconnect(static_cast<int>(key));
				if (ex_over->_comp_type == OP_SEND) delete ex_over;
				continue;
			}
		}

		if ((0 == num_bytes) && ((ex_over->_comp_type == OP_RECV) || (ex_over->_comp_type == OP_SEND))) {
			disconnect(static_cast<int>(key));
			if (ex_over->_comp_type == OP_SEND) delete ex_over;
			continue;
		}

		switch (ex_over->_comp_type) {
		case OP_ACCEPT: {
			int client_id = get_new_client_id();
			if (client_id != -1) {
				{
					lock_guard<mutex> ll(clients[client_id]->_s_lock);
					clients[client_id]->_state = ST_ALLOC;
				}
				clients[client_id]->x = 0;
				clients[client_id]->y = 0;
				clients[client_id]->_id = client_id;
				clients[client_id]->_name[0] = 0;
				clients[client_id]->_prev_remain = 0;
				clients[client_id]->_socket = g_c_socket;
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_c_socket),
					h_iocp, client_id, 0);
				clients[client_id]->do_recv();
				g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
				cout << client_id << endl;
			}
			else {
				cout << "Max user exceeded.\n";
			}
			ZeroMemory(&g_a_over._over, sizeof(g_a_over._over));
			int addr_size = sizeof(SOCKADDR_IN);
			AcceptEx(g_s_socket, g_c_socket, g_a_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &g_a_over._over);
			break;
		}
		case OP_RECV: {
			int remain_data = num_bytes + clients[key]->_prev_remain;
			char* p = ex_over->_send_buf;
			while (remain_data > 0) {
				int packet_size = p[0];
				if (packet_size <= remain_data) {
					process_packet(static_cast<int>(key), p);
					p = p + packet_size;
					remain_data = remain_data - packet_size;
				}
				else break;
			}
			clients[key]->_prev_remain = remain_data;
			if (remain_data > 0) {
				memcpy(ex_over->_send_buf, p, remain_data);
			}
			clients[key]->do_recv();
			break;
		}
		case OP_SEND:
			delete ex_over;
			break;
		case OP_NPC_MOVE: {
			bool keep_alive = false;
			for (int j = 0; j < MAX_USER; ++j) {
				if (clients[j]->_state != ST_INGAME) continue;
				if (can_see(static_cast<int>(key), j)) {
					keep_alive = true;
					break;
				}
			}
			if (true == keep_alive) {
				do_npc_random_move(static_cast<int>(key));
				TIMER_EVENT ev{ key, chrono::system_clock::now() + 1s, EV_RANDOM_MOVE, 0 };
				timer_queue.push(ev);
			}
			else {
				clients[key]->_is_active = false;
			}
			delete ex_over;
		}
						break;
		case OP_AI_HELLO: {
			clients[key]->_ll.lock();
			auto L = clients[key]->_L;
			lua_getglobal(L, "event_player_move");
			lua_pushnumber(L, ex_over->_ai_target_obj);
			lua_pcall(L, 1, 0, 0);
			//lua_pop(L, 1);
			clients[key]->_ll.unlock();
			delete ex_over;
		}
						break;

		}
	}
}
int API_get_x(lua_State* L)
{
	int user_id =
		(int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int x = clients[user_id]->x;
	lua_pushnumber(L, x);
	return 1;
}
int API_get_y(lua_State* L)
{
	int user_id =
		(int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int y = clients[user_id]->y;
	lua_pushnumber(L, y);
	return 1;
}
int API_SendMessage(lua_State* L)
{
	int my_id = (int)lua_tointeger(L, -3);
	int user_id = (int)lua_tointeger(L, -2);
	char* mess = (char*)lua_tostring(L, -1);

	lua_pop(L, 4);

	clients[user_id]->send_chat_packet(my_id, mess);
	return 0;
}
int API_Move(lua_State* L)
{
	int npc_id = (int)lua_tointeger(L, -2);
	int dir = (int)lua_tointeger(L, -1);
	lua_pop(L, 3);

	auto& npc = clients[npc_id];
	if (!npc) return 0;

	int x = npc->x;
	int y = npc->y;
	switch (dir) {
	case 0: if (y > 0) y--; break;           // ↑
	case 1: if (y < MAP_HEIGHT - 1) y++; break; // ↓
	case 2: if (x > 0) x--; break;           // ←
	case 3: if (x < MAP_WIDTH - 1) x++; break; // →
	}
	npc->x = x;
	npc->y = y;

	// 플레이어들에게 위치 갱신 전송 필요시 여기에 코드 추가

	return 0;
}

void InitializeNPC()
{
	cout << "NPC initialize begin.\n";

	for (int i = 0; i < MAX_USER; ++i)
		clients[i] = std::make_shared<SESSION>();

	for (int i = MAX_USER; i < MAX_USER + NUM_MONSTER; ++i) {
		auto session = std::make_shared<SESSION>();
		session->x = rand() % MAP_WIDTH;
		session->y = rand() % MAP_HEIGHT;
		session->_id = i;
		sprintf_s(session->_name, "NPC%d", i);
		session->_state = ST_INGAME;

		auto L = session->_L = luaL_newstate();
		luaL_openlibs(L);

		if (luaL_loadfile(L, "npc.lua") || lua_pcall(L, 0, 0, 0)) {
			std::cerr << "NPC " << i << " failed to load npc.lua: " << lua_tostring(L, -1) << "\n";
			lua_close(L);
			continue; // 다음 NPC로 넘어감
		}

		lua_getglobal(L, "set_uid");
		lua_pushnumber(L, i);
		if (lua_pcall(L, 1, 0, 0) != 0) {
			std::cerr << "NPC " << i << " set_uid call failed: " << lua_tostring(L, -1) << "\n";
		}

		lua_register(L, "API_SendMessage", API_SendMessage);
		lua_register(L, "API_get_x", API_get_x);
		lua_register(L, "API_get_y", API_get_y);
		lua_register(L, "API_Move", API_Move);

		clients[i] = session; // 최종적으로 clients에 삽입
	}

	cout << "NPC initialize end.\n";
}
void do_timer() {
	while (true) {
		TIMER_EVENT ev;
		auto current_time = chrono::system_clock::now();
		if (true == timer_queue.try_pop(ev)) {
			if (ev.wakeup_time > current_time) {
				timer_queue.push(ev);
				this_thread::sleep_for(1ms);
				continue;
			}
			switch (ev.event_id) {
			case EV_RANDOM_MOVE:
			{
				OVER_EXP* ov = new OVER_EXP;
				ov->_comp_type = OP_NPC_MOVE;
				PostQueuedCompletionStatus(h_iocp, 1, ev.obj_id, &ov->_over);
				break;
			}
			case EV_HP_REGEN: // HP 회복 이벤트 처리
			{
				int player_id = ev.obj_id;
				auto& player = clients[player_id];

				// 플레이어가 접속 중이고 게임 내에 있는지 확인
				if (player && player->_state == ST_INGAME) {
					lock_guard<mutex> ll(player->_s_lock);
					int current_hp = player->_hp;
					int max_hp = player->_level * 100;

					// HP 10% 회복 계산
					int regen_amount = max(1, max_hp / 10); // 최소 1 회복, 최대 HP의 10%
					int new_hp = min(max_hp, current_hp + regen_amount);

					if (new_hp != current_hp) {
						player->_hp = new_hp;
						// 클라이언트에게 변경된 HP 정보 전송
						player->send_stat_change(player_id, new_hp, player->_exp, player->_level);
						//cout << "Player " << player_id << " HP regenerated to " << new_hp << endl;
					}

					// 다음 HP 회복 이벤트를 5초 뒤로 예약
					TIMER_EVENT next_regen_ev{ player_id, chrono::system_clock::now() + 5s, EV_HP_REGEN, 0 };
					timer_queue.push(next_regen_ev);
				}
				break;
			}
			continue;
			}
		}
		this_thread::sleep_for(1ms);
	}
}

int main()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	g_s_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(GAME_PORT);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	bind(g_s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(g_s_socket, SOMAXCONN);
	SOCKADDR_IN cl_addr;
	int addr_size = sizeof(cl_addr);

	InitializeNPC();

	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_s_socket), h_iocp, 9999, 0);
	g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_a_over._comp_type = OP_ACCEPT;
	AcceptEx(g_s_socket, g_c_socket, g_a_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &g_a_over._over);

	vector <thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread, h_iocp);
	thread timer_thread{ do_timer };
	timer_thread.join();
	for (auto& th : worker_threads)
		th.join();
	closesocket(g_s_socket);
	WSACleanup();
}
