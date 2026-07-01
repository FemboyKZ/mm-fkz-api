#ifndef _INCLUDE_PLAYER_MANAGER_H_
#define _INCLUDE_PLAYER_MANAGER_H_

#include <cstdint>
#include "globals.h"

struct PlayerInfo
{
	bool connected;
	bool inGame;
	bool isBot;
	uint64_t steamId64;
	char name[128];
	char ipAddress[64];
	double connectTime; // Plat_FloatTime() when connected
};

class PlayerManager
{
public:
	void Reset();
	void ResetPlayer(int slot);

	void OnClientConnected(int slot, const char *name, uint64_t xuid, const char *address, bool fakePlayer);
	void OnClientPutInServer(int slot, const char *name, int type, uint64_t xuid);
	void OnClientDisconnect(int slot);

	int GetHumanPlayerCount() const;
	int GetBotCount() const;

	const PlayerInfo &GetPlayer(int slot) const
	{
		return m_players[slot];
	}

private:
	PlayerInfo m_players[MAXPLAYERS + 1];
};

extern PlayerManager g_PlayerManager;

#endif // _INCLUDE_PLAYER_MANAGER_H_
