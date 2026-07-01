#include <cstring>

#include "player_manager.h"
#include <tier0/platform.h>

PlayerManager g_PlayerManager;

void PlayerManager::Reset()
{
	memset(m_players, 0, sizeof(m_players));
}

void PlayerManager::ResetPlayer(int slot)
{
	memset(&m_players[slot], 0, sizeof(PlayerInfo));
}

void PlayerManager::OnClientConnected(int slot, const char *name, uint64_t xuid, const char *address, bool fakePlayer)
{
	if (slot < 0 || slot >= MAXPLAYERS)
	{
		return;
	}

	m_players[slot].connected = true;
	m_players[slot].inGame = false;
	m_players[slot].isBot = fakePlayer;
	m_players[slot].steamId64 = xuid;
	m_players[slot].connectTime = Plat_FloatTime();

	strncpy(m_players[slot].name, name ? name : "", sizeof(m_players[slot].name) - 1);
	m_players[slot].name[sizeof(m_players[slot].name) - 1] = '\0';

	// Store IP address (address may be "ip:port" format)
	m_players[slot].ipAddress[0] = '\0';
	if (address && !fakePlayer)
	{
		strncpy(m_players[slot].ipAddress, address, sizeof(m_players[slot].ipAddress) - 1);
		m_players[slot].ipAddress[sizeof(m_players[slot].ipAddress) - 1] = '\0';
		// Strip port if present
		char *colon = strrchr(m_players[slot].ipAddress, ':');
		if (colon)
		{
			*colon = '\0';
		}
	}
}

void PlayerManager::OnClientPutInServer(int slot, const char *name, int type, uint64_t xuid)
{
	if (slot < 0 || slot >= MAXPLAYERS)
	{
		return;
	}

	m_players[slot].connected = true;
	m_players[slot].inGame = true;
	m_players[slot].isBot = (type == 1);
	m_players[slot].steamId64 = xuid;

	if (name)
	{
		strncpy(m_players[slot].name, name, sizeof(m_players[slot].name) - 1);
		m_players[slot].name[sizeof(m_players[slot].name) - 1] = '\0';
	}

	// Set connect time if not already set (may have been set in OnClientConnected)
	if (m_players[slot].connectTime <= 0.0)
	{
		m_players[slot].connectTime = Plat_FloatTime();
	}
}

void PlayerManager::OnClientDisconnect(int slot)
{
	if (slot < 0 || slot >= MAXPLAYERS)
	{
		return;
	}
	ResetPlayer(slot);
}

int PlayerManager::GetHumanPlayerCount() const
{
	int count = 0;
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (m_players[i].connected && m_players[i].inGame && !m_players[i].isBot)
		{
			count++;
		}
	}
	return count;
}

int PlayerManager::GetBotCount() const
{
	int count = 0;
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (m_players[i].connected && m_players[i].isBot)
		{
			count++;
		}
	}
	return count;
}
