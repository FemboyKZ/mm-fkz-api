/**
 * FKZ API Plugin
 *
 * Exposes FKZ API calls as natives.
 * Reports server/player data to API via periodic POST.
 */

#include "fkzapi.h"
#include "api/api.h"
#include "config/config.h"
#include "chat/cross_chat.h"
#include "kz/cs2kz.h"
#include "lang/translations.h"
#include "db/database.h"
#include "common.h"
#include "http/http_client.h"
#include "report/json_builder.h"
#include "player/player_manager.h"
#include "report/reporter.h"
#include "report/server_info.h"

#include "game/cvarquery.h"
#include "utils/log.h"

#include <ISmmAPI.h>
#include <cstring>
#include <engine2/isource2server.h>
#include <engine2/ivengineserver2.h>
#include <engine2/igameeventsystem.h>
#include <icvar.h>
#include "interfaces/cs2kz/ics2kz.h"
#include <engine2/gamesessionconfiguration.h>
#include <engine2/inetworkserverservice.h>
#include <engine2/networkgameserverbase.h>
#include <networksystem/inetworkmessages.h>
#include <tier0/platform.h>

// The other interfaces in common.h come with the SDK's interfaces lib.
CGlobalVars *g_pGlobals = nullptr;
IGameEventSystem *g_pGameEventSystem = nullptr;
// For mmu::cvarquery.
IVEngineServer *g_pEngine = nullptr;

FKZPlugin g_ThisPlugin;
PLUGIN_EXPOSE(FKZPlugin, g_ThisPlugin);

// Players already on the server at a late load never pass through the connect hooks,
// and without them the server reports as empty and cross-chat skips them.
static void AddExistingPlayers()
{
	CS2KZ_Refresh();
	g_PlayerManager.OnLateLoad();
	for (int slot = 0; slot < MAXPLAYERS; slot++)
	{
		PlayerInfo *p = g_PlayerManager.Get(slot);
		if (!p)
		{
			continue;
		}
		// The engine has no name lookup by slot, cs2kz does. Without it the name fills in on the next connect.
		p->name = g_pCS2KZ ? g_pCS2KZ->GetPlayerName(slot) : "";
		p->connectTime = Plat_FloatTime();
		mmu::cvarquery::OnClientConnected(slot, false);
		CS2KZ_ResetPlayer(slot);
	}
}

FKZPlugin::FKZPlugin()
	: m_GameFrame(&ISource2Server::GameFrame, this, nullptr, &FKZPlugin::Hook_GameFrame),
	  m_ServerHibernationUpdate(&ISource2Server::ServerHibernationUpdate, this, nullptr, &FKZPlugin::Hook_ServerHibernationUpdate),
	  m_ClientPutInServer(&ISource2GameClients::ClientPutInServer, this, nullptr, &FKZPlugin::Hook_ClientPutInServer),
	  m_ClientDisconnect(&ISource2GameClients::ClientDisconnect, this, nullptr, &FKZPlugin::Hook_ClientDisconnect),
	  m_OnClientConnected(&ISource2GameClients::OnClientConnected, this, nullptr, &FKZPlugin::Hook_OnClientConnected),
	  m_StartupServer(&INetworkServerService::StartupServer, this, nullptr, &FKZPlugin::Hook_StartupServer),
	  m_DispatchConCommand(&ICvar::DispatchConCommand, this, &FKZPlugin::Hook_DispatchConCommand, nullptr)
{
}

// cfg/fkz-api/core.cfg, or the one alongside the plugin's addons folder.
static void FKZ_ReloadConfig()
{
	const std::string base = g_SMAPI->GetBaseDir();
	const std::string path = base + "/cfg/" PLUGIN_NAME "/core.cfg";
	if (!FKZ_LoadConfig(path.c_str(), g_FKZConfig) && !FKZ_LoadConfig((base + "/addons/" PLUGIN_NAME "/core.cfg").c_str(), g_FKZConfig))
	{
		MMU_LOG_WARN("No config read from %s. It has to be \"" PLUGIN_NAME "\" { ... } with its sections, "
					 "the key \"value\" lines of older versions are not read.\n",
					 path.c_str());
	}

	g_FKZTranslations.Load(g_SMAPI->GetBaseDir(), PLUGIN_NAME);
	g_FKZTranslations.SetDefaultLanguage(g_FKZConfig.general.defaultLanguage);
}

bool FKZPlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	m_bSkipLevelInitReload = !late;
	PLUGIN_SAVEVARS();

	mmu::log::Init("FKZ", PLUGIN_NAME);

	// ismm->MetaFactory(FKZAPI_INTERFACE) -> our OnMetamodQuery().
	ismm->AddListener(this, this);

	GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2Server, ISource2Server, INTERFACEVERSION_SERVERGAMEDLL);
	GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2GameClients, ISource2GameClients, INTERFACEVERSION_SERVERGAMECLIENTS);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pEngineServer, IVEngineServer2, INTERFACEVERSION_VENGINESERVER);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pEngine, IVEngineServer, INTERFACEVERSION_VENGINESERVER);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	META_CONVAR_REGISTER(FCVAR_RELEASE | FCVAR_GAMEDLL);

	// Non fatal, translations fall back to the default language.
	mmu::cvarquery::Init(g_pEngine);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkMessages, INetworkMessages, NETWORKMESSAGES_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pGameEventSystem, IGameEventSystem, GAMEEVENTSYSTEM_INTERFACE_VERSION);

	m_lastReportTime = 0.0;
	m_serverActive = false;
	m_hibernateSent = false;

	FKZ_ReloadConfig();

	if (g_FKZConfig.apiUrl.empty())
	{
		MMU_LOG_WARN("No api_url configured, reporting disabled\n");
	}
	else
	{
		MMU_LOG_INFO("v%s loaded - reporting to %s every %.0fs (key=%s)\n", GetVersion(), g_FKZConfig.apiUrl.c_str(), g_FKZConfig.interval,
					 !g_FKZConfig.apiKey.empty() ? "set" : "NOT SET");
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

bool FKZPlugin::Unload(char *error, size_t maxlen)
{
	ConVar_Unregister();
	m_GameFrame.Remove(g_pSource2Server);
	m_ServerHibernationUpdate.Remove(g_pSource2Server);
	m_ClientPutInServer.Remove(g_pSource2GameClients);
	m_ClientDisconnect.Remove(g_pSource2GameClients);
	m_OnClientConnected.Remove(g_pSource2GameClients);
	m_StartupServer.Remove(g_pNetworkServerService);
	m_DispatchConCommand.Remove(g_pCVar);

	mmu::cvarquery::Shutdown();
	g_HttpClient.ReleasePending();
	FKZ_Database_Cleanup();

	mmu::log::Shutdown();

	return true;
}

void FKZPlugin::AllPluginsLoaded()
{
	g_HttpClient.Init();
	FKZ_Database_Init();
	CS2KZ_Refresh();
}

void FKZPlugin::OnPluginLoad(PluginId /*id*/)
{
	CS2KZ_Refresh();
}

void FKZPlugin::OnPluginUnload(PluginId id)
{
	CS2KZ_Refresh();
	g_FKZApi.DropOwnedBy(id);
}

void *FKZPlugin::OnMetamodQuery(const char *iface, int *ret)
{
	if (iface && strcmp(iface, FKZAPI_INTERFACE) == 0)
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

CON_COMMAND_F(mm_fkz_reload, "Reload fkz-api core.cfg.", FCVAR_GAMEDLL | FCVAR_RELEASE)
{
	FKZ_ReloadConfig();
}

CON_COMMAND_F(mm_fkz_version, "Print the plugin version.", FCVAR_GAMEDLL | FCVAR_RELEASE)
{
	MMU_LOG_INFO("Plugin version %s, interface %s.\n", PLUGIN_FULL_VERSION, FKZAPI_INTERFACE);
}

// Hooks

void FKZPlugin::OnLevelInit(char const *, char const *, char const *, char const *, bool, bool)
{
	// Load has just read it for the first map.
	if (m_bSkipLevelInitReload)
	{
		m_bSkipLevelInitReload = false;
		return;
	}
	FKZ_ReloadConfig();
}

KHook::Return<void> FKZPlugin::Hook_StartupServer(INetworkServerService *, const GameSessionConfiguration_t &config, ISource2WorldSession *,
												  const char *)
{
	g_pGlobals = g_SMAPI->GetCGlobals();
	m_serverActive = true;
	m_hibernateSent = false;

	g_HttpClient.Init();
	g_ServerInfo.Cache();

	// Reset report timer, first report after short delay
	m_lastReportTime = Plat_FloatTime() + 2.0 - g_FKZConfig.interval;

	MMU_LOG_INFO("Server started, reporting active\n");
	return {KHook::Action::Ignore};
}

KHook::Return<void> FKZPlugin::Hook_OnClientConnected(ISource2GameClients *, CPlayerSlot slot, const char *pszName, uint64 xuid,
													  const char *pszNetworkID, const char *pszAddress, bool bFakePlayer)
{
	g_PlayerManager.OnClientConnected(slot.Get(), pszName, xuid, pszAddress, bFakePlayer);
	mmu::cvarquery::OnClientConnected(slot.Get(), bFakePlayer);
	return {KHook::Action::Ignore};
}

KHook::Return<void> FKZPlugin::Hook_ClientPutInServer(ISource2GameClients *, CPlayerSlot slot, char const *pszName, int type, uint64 xuid)
{
	g_PlayerManager.OnClientPutInServer(slot.Get());
	CS2KZ_ResetPlayer(slot.Get());

	// Load saved cross-chat state once the player has a real steamID.
	if (type != 1)
	{
		FKZ_CrossChat_LoadPrefs(slot.Get(), xuid);
	}

	if (type != 1 && !g_FKZConfig.apiUrl.empty() && g_PlayerManager.GetHumanPlayerCount() == 1)
	{
		m_lastReportTime = 0.0;
		m_hibernateSent = false;
	}
	return {KHook::Action::Ignore};
}

KHook::Return<void> FKZPlugin::Hook_ClientDisconnect(ISource2GameClients *, CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName,
													 uint64 xuid, const char *pszNetworkID)
{
	int s = slot.Get();
	const PlayerInfo *player = g_PlayerManager.Get(s);
	bool wasInGame = player && player->inGame && !player->fakePlayer;

	// The slot is cleared right below, so whatever this player accrued since the last report has to go out first.
	if (wasInGame && !g_FKZConfig.apiUrl.empty())
	{
		CS2KZ_SampleAll();
		if (CS2KZ_HasPendingPlaytime(s))
		{
			FKZ_SendReport();
			m_lastReportTime = Plat_FloatTime();
		}
	}

	g_PlayerManager.OnClientDisconnect(s);
	mmu::cvarquery::OnClientDisconnect(s);
	FKZ_CrossChat_OnClientDisconnect(s);
	CS2KZ_ResetPlayer(s);

	// Send hibernate signal when last human player leaves
	if (wasInGame && g_PlayerManager.GetHumanPlayerCount() == 0 && !g_FKZConfig.apiUrl.empty() && !m_hibernateSent)
	{
		m_hibernateSent = true;
		FKZ_SendHibernate();
	}
	return {KHook::Action::Ignore};
}

KHook::Return<void> FKZPlugin::Hook_ServerHibernationUpdate(ISource2Server *, bool bHibernating)
{
	if (!bHibernating)
	{
		m_hibernateSent = false;
	}
	// The last player leaving reaches us through ClientDisconnect as well, and one signal per emptying is enough.
	else if (!m_hibernateSent && !g_FKZConfig.apiUrl.empty())
	{
		m_hibernateSent = true;
		FKZ_SendHibernate();
	}
	return {KHook::Action::Ignore};
}

KHook::Return<void> FKZPlugin::Hook_GameFrame(ISource2Server *, bool simulating, bool bFirstTick, bool bLastTick)
{
	FKZ_Database_RunFrame();

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

	if (g_FKZConfig.apiUrl.empty())
	{
		return {KHook::Action::Ignore};
	}

	int humans = g_PlayerManager.GetHumanPlayerCount();

	// Keep the cross-chat long-poll open while players are present.
	FKZ_CrossChat_Tick(humans > 0);

	// Poll each player's cs2kz mode so the per-mode playtime follows mode switches between reports.
	CS2KZ_Tick();

	// Don't report while idle (no human players / server hibernating).
	if (humans == 0)
	{
		return {KHook::Action::Ignore};
	}

	double now = Plat_FloatTime();
	if (now - m_lastReportTime >= g_FKZConfig.interval)
	{
		m_lastReportTime = now;
		FKZ_SendReport();
	}
	return {KHook::Action::Ignore};
}

KHook::Return<void> FKZPlugin::Hook_DispatchConCommand(ICvar *, ConCommandRef cmd, const CCommandContext &ctx, const CCommand &args)
{
	return {FKZ_CrossChat_OnDispatchConCommand(cmd, ctx, args) ? KHook::Action::Supersede : KHook::Action::Ignore};
}
