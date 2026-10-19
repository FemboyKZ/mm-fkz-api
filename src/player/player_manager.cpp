#include "player/player_manager.h"
#include <tier0/platform.h>

PlayerManager g_PlayerManager;

void PlayerManager::OnClientConnected(int slot, const char *name, uint64_t xuid, const char *address, bool fakePlayer)
{
	Players::OnClientConnected(slot, name, xuid, address, fakePlayer);
	if (PlayerInfo *p = Get(slot))
	{
		p->connectTime = Plat_FloatTime();
	}
}

int PlayerManager::GetHumanPlayerCount() const
{
	return Count([](const PlayerInfo &p) { return p.inGame && !p.fakePlayer; });
}

int PlayerManager::GetBotCount() const
{
	return Count([](const PlayerInfo &p) { return p.fakePlayer; });
}
