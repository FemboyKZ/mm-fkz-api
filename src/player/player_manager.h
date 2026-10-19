#ifndef _INCLUDE_FKZ_PLAYER_MANAGER_H_
#define _INCLUDE_FKZ_PLAYER_MANAGER_H_

#include "common.h"
#include "game/players.h"

struct PlayerInfo : mmu::Player
{
	double connectTime = 0.0; // Plat_FloatTime() when connected
};

class PlayerManager : public mmu::Players<PlayerInfo>
{
public:
	void OnClientConnected(int slot, const char *name, uint64_t xuid, const char *address, bool fakePlayer);

	int GetHumanPlayerCount() const;
	int GetBotCount() const;
};

extern PlayerManager g_PlayerManager;

#endif // _INCLUDE_FKZ_PLAYER_MANAGER_H_
