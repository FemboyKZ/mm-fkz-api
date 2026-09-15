/**
 * FKZ API Plugin
 *
 * Exposes FKZ API calls as natives.
 * Reports server/player data to API via periodic POST.
 */

#include "plugin.h"
#include "api.h"
#include "config.h"
#include "cross_chat.h"
#include "cs2kz.h"
#include "database.h"
#include "globals.h"
#include "http_client.h"
#include "json_builder.h"
#include "player_manager.h"
#include "reporter.h"
#include "server_info.h"

#include <ISmmAPI.h>
#include <cstring>
#include <eiface.h>
#include <engine/igameeventsystem.h>
#include <icvar.h>
#include <ics2kz.h>
#include <inetchannelinfo.h>
#include <iserver.h>
#include <networksystem/inetworkmessages.h>
#include <tier0/platform.h>

class GameSessionConfiguration_t
{
};

MMSPlugin g_ThisPlugin;
PLUGIN_EXPOSE(MMSPlugin, g_ThisPlugin);

// Players already on the server at a late load never pass through the connect hooks,
// and without them the server reports as empty and cross-chat skips them.
static void AddExistingPlayers()
{
	CS2KZ_Refresh();
	for (int slot = 0; slot < MAXPLAYERS; slot++)
	{
		CPlayerSlot playerSlot(slot);
		uint64 xuid = g_pEngineServer->GetClientXUID(playerSlot);
		INetChannelInfo *netInfo = g_pEngineServer->GetPlayerNetInfo(playerSlot);
		if (xuid == 0 || !netInfo)
		{
			continue;
		}
		// The engine has no name lookup by slot, cs2kz does. Without it the name fills in on the next connect.
		const char *name = g_pCS2KZ ? g_pCS2KZ->GetPlayerName(slot) : "";
		g_PlayerManager.OnClientConnected(slot, name, xuid, netInfo->GetAddress(), false);
		g_PlayerManager.OnClientPutInServer(slot, name, 0, xuid);
		CS2KZ_ResetPlayer(slot);
	}
}

MMSPlugin::MMSPlugin()
	: m_GameFrame(&ISource2Server::GameFrame, this, nullptr, &MMSPlugin::Hook_GameFrame),
	  m_ServerHibernationUpdate(&ISource2Server::ServerHibernationUpdate, this, nullptr, &MMSPlugin::Hook_ServerHibernationUpdate),
	  m_ClientPutInServer(&ISource2GameClients::ClientPutInServer, this, nullptr, &MMSPlugin::Hook_ClientPutInServer),
	  m_ClientDisconnect(&ISource2GameClients::ClientDisconnect, this, nullptr, &MMSPlugin::Hook_ClientDisconnect),
	  m_OnClientConnected(&ISource2GameClients::OnClientConnected, this, nullptr, &MMSPlugin::Hook_OnClientConnected),
	  m_StartupServer(&INetworkServerService::StartupServer, this, nullptr, &MMSPlugin::Hook_StartupServer),
	  m_DispatchConCommand(&ICvar::DispatchConCommand, this, &MMSPlugin::Hook_DispatchConCommand, nullptr)
{
}

bool MMSPlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	// ismm->MetaFactory(FKZ_API_INTERFACE) -> our OnMetamodQuery().
	ismm->AddListener(this, this);

	GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2Server, ISource2Server, INTERFACEVERSION_SERVERGAMEDLL);
	GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2GameClients, ISource2GameClients, INTERFACEVERSION_SERVERGAMECLIENTS);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pEngineServer, IVEngineServer2, INTERFACEVERSION_VENGINESERVER);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkMessages, INetworkMessages, NETWORKMESSAGES_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pGameEventSystem, IGameEventSystem, GAMEEVENTSYSTEM_INTERFACE_VERSION);

	g_PlayerManager.Reset();
	m_lastReportTime = 0.0;
	m_serverActive = false;
	m_hibernateSent = false;

	g_Config.Load();

	if (g_Config.apiUrl[0] == '\0')
	{
		META_CONPRINTF("[FKZ] No api_url configured, reporting disabled\n");
	}
	else
	{
		META_CONPRINTF("[FKZ] v%s loaded - reporting to %s every %.0fs (key=%s)\n", GetVersion(), g_Config.apiUrl, g_Config.interval,
					   g_Config.apiKey[0] != '\0' ? "set" : "NOT SET");
	}

	// Register hooks
	m_GameFrame.Add(g_pSource2Server);
	m_ServerHibernationUpdate.Add(g_pSource2Server);
	m_ClientPutInServer.Add(g_pSource2GameClients);
	m_ClientDisconnect.Add(g_pSource2GameClients);
	m_OnClientConnected.Add(g_pSource2GameClients);
	m_StartupServer.Add(g_pNetworkServerService);
	m_DispatchConCommand.Add(g_pCVar);

	// Late load: server is already running
	if (late)
	{
		g_pGlobals = ismm->GetCGlobals();
		g_HttpClient.Init();
		m_serverActive = true;
		g_ServerInfo.Cache();
		m_lastReportTime = Plat_FloatTime();
		AddExistingPlayers();
	}

	return true;
}

bool MMSPlugin::Unload(char *error, size_t maxlen)
{
	m_GameFrame.Remove(g_pSource2Server);
	m_ServerHibernationUpdate.Remove(g_pSource2Server);
	m_ClientPutInServer.Remove(g_pSource2GameClients);
	m_ClientDisconnect.Remove(g_pSource2GameClients);
	m_OnClientConnected.Remove(g_pSource2GameClients);
	m_StartupServer.Remove(g_pNetworkServerService);
	m_DispatchConCommand.Remove(g_pCVar);

	g_HttpClient.ReleasePending();
	Database_Cleanup();

	return true;
}

void MMSPlugin::AllPluginsLoaded()
{
	g_HttpClient.Init();
	Database_Init();
	CS2KZ_Refresh();
}

void MMSPlugin::OnPluginLoad(PluginId /*id*/)
{
	CS2KZ_Refresh();
}

void MMSPlugin::OnPluginUnload(PluginId /*id*/)
{
	CS2KZ_Refresh();
}

void *MMSPlugin::OnMetamodQuery(const char *iface, int *ret)
{
	if (iface && strcmp(iface, FKZ_API_INTERFACE) == 0)
	{
		if (ret)
		{
			*ret = META_IFACE_OK;
		}
		return static_cast<void *>(static_cast<IFKZApi *>(&g_FKZApi));
	}

	if (ret)
	{
		*ret = META_IFACE_FAILED;
	}
	return NULL;
}

// Hooks

KHook::Return<void> MMSPlugin::Hook_StartupServer(INetworkServerService *, const GameSessionConfiguration_t &config, ISource2WorldSession *,
												  const char *)
{
	g_pGlobals = g_SMAPI->GetCGlobals();
	m_serverActive = true;
	m_hibernateSent = false;

	g_HttpClient.Init();
	g_ServerInfo.Cache();

	// Reset report timer, first report after short delay
	m_lastReportTime = Plat_FloatTime() + 2.0 - g_Config.interval;

	META_CONPRINTF("[FKZ] Server started, reporting active\n");
	return {KHook::Action::Ignore};
}

KHook::Return<void> MMSPlugin::Hook_OnClientConnected(ISource2GameClients *, CPlayerSlot slot, const char *pszName, uint64 xuid,
													  const char *pszNetworkID, const char *pszAddress, bool bFakePlayer)
{
	g_PlayerManager.OnClientConnected(slot.Get(), pszName, xuid, pszAddress, bFakePlayer);
	return {KHook::Action::Ignore};
}

KHook::Return<void> MMSPlugin::Hook_ClientPutInServer(ISource2GameClients *, CPlayerSlot slot, char const *pszName, int type, uint64 xuid)
{
	g_PlayerManager.OnClientPutInServer(slot.Get(), pszName, type, xuid);
	CS2KZ_ResetPlayer(slot.Get());

	// Load saved cross-chat state once the player has a real steamID.
	if (type != 1)
	{
		CrossChat_LoadPrefs(slot.Get(), xuid);
	}

	if (type != 1 && g_Config.apiUrl[0] != '\0' && g_PlayerManager.GetHumanPlayerCount() == 1)
	{
		m_lastReportTime = 0.0;
		m_hibernateSent = false;
	}
	return {KHook::Action::Ignore};
}

KHook::Return<void> MMSPlugin::Hook_ClientDisconnect(ISource2GameClients *, CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName,
													 uint64 xuid, const char *pszNetworkID)
{
	int s = slot.Get();
	const PlayerInfo &player = g_PlayerManager.GetPlayer(s);
	bool wasInGame = player.connected && player.inGame && !player.isBot;

	// The slot is cleared right below, so whatever this player accrued since the last report has to go out first.
	if (wasInGame && g_Config.apiUrl[0] != '\0')
	{
		CS2KZ_SampleAll();
		if (CS2KZ_HasPendingPlaytime(s))
		{
			SendReport();
			m_lastReportTime = Plat_FloatTime();
		}
	}

	g_PlayerManager.OnClientDisconnect(s);
	CrossChat_OnClientDisconnect(s);
	CS2KZ_ResetPlayer(s);

	// Send hibernate signal when last human player leaves
	if (wasInGame && g_PlayerManager.GetHumanPlayerCount() == 0 && g_Config.apiUrl[0] != '\0' && !m_hibernateSent)
	{
		m_hibernateSent = true;
		SendHibernate();
	}
	return {KHook::Action::Ignore};
}

KHook::Return<void> MMSPlugin::Hook_ServerHibernationUpdate(ISource2Server *, bool bHibernating)
{
	if (!bHibernating)
	{
		m_hibernateSent = false;
	}
	// The last player leaving reaches us through ClientDisconnect as well, and one signal per emptying is enough.
	else if (!m_hibernateSent && g_Config.apiUrl[0] != '\0')
	{
		m_hibernateSent = true;
		SendHibernate();
	}
	return {KHook::Action::Ignore};
}

KHook::Return<void> MMSPlugin::Hook_GameFrame(ISource2Server *, bool simulating, bool bFirstTick, bool bLastTick)
{
	Database_RunFrame();

	if (!m_serverActive)
	{
		return {KHook::Action::Ignore};
	}

	if (!g_HttpClient.IsReady())
	{
		g_HttpClient.Init();
	}

	// Pump Steam callbacks so HTTP completion handlers fire,
	// both the status updater's and any responses for the public API consumers.
	g_HttpClient.RunCallbacks();

	if (g_Config.apiUrl[0] == '\0')
	{
		return {KHook::Action::Ignore};
	}

	int humans = g_PlayerManager.GetHumanPlayerCount();

	// Keep the cross-chat long-poll open while players are present.
	CrossChat_Tick(humans > 0);

	// Poll each player's cs2kz mode so the per-mode playtime follows mode switches between reports.
	CS2KZ_Tick();

	// Don't report while idle (no human players / server hibernating).
	if (humans == 0)
	{
		return {KHook::Action::Ignore};
	}

	double now = Plat_FloatTime();
	if (now - m_lastReportTime >= g_Config.interval)
	{
		m_lastReportTime = now;
		SendReport();
	}
	return {KHook::Action::Ignore};
}

KHook::Return<void> MMSPlugin::Hook_DispatchConCommand(ICvar *, ConCommandRef cmd, const CCommandContext &ctx, const CCommand &args)
{
	return {CrossChat_OnDispatchConCommand(cmd, ctx, args) ? KHook::Action::Supersede : KHook::Action::Ignore};
}
