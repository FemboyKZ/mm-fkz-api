/**
 * FKZ API Plugin
 *
 * Exposes FKZ API calls as natives.
 * Reports server/player data to API via periodic POST.
 */

#include "plugin.h"
#include "api.h"
#include "config.h"
#include "globals.h"
#include "http_client.h"
#include "json_builder.h"
#include "player_manager.h"
#include "reporter.h"
#include "server_info.h"

#include <ISmmAPI.h>
#include <cstring>
#include <eiface.h>
#include <iserver.h>
#include <tier0/platform.h>

class GameSessionConfiguration_t {};

MMSPlugin g_ThisPlugin;
PLUGIN_EXPOSE(MMSPlugin, g_ThisPlugin);

SH_DECL_HOOK3_void(ISource2Server, GameFrame, SH_NOATTRIB, 0, bool, bool, bool);
SH_DECL_HOOK1_void(ISource2Server, ServerHibernationUpdate, SH_NOATTRIB, 0,
                   bool);
SH_DECL_HOOK4_void(ISource2GameClients, ClientPutInServer, SH_NOATTRIB, 0,
                   CPlayerSlot, char const *, int, uint64);
SH_DECL_HOOK5_void(ISource2GameClients, ClientDisconnect, SH_NOATTRIB, 0,
                   CPlayerSlot, ENetworkDisconnectionReason, const char *,
                   uint64, const char *);
SH_DECL_HOOK6_void(ISource2GameClients, OnClientConnected, SH_NOATTRIB, 0,
                   CPlayerSlot, const char *, uint64, const char *,
                   const char *, bool);
SH_DECL_HOOK3_void(INetworkServerService, StartupServer, SH_NOATTRIB, 0,
                   const GameSessionConfiguration_t &, ISource2WorldSession *,
                   const char *);

bool MMSPlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen,
                     bool late) {
  PLUGIN_SAVEVARS();

  // ismm->MetaFactory(FKZ_API_INTERFACE) -> our OnMetamodQuery().
  ismm->AddListener(this, this);

  GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2Server, ISource2Server,
                      INTERFACEVERSION_SERVERGAMEDLL);
  GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2GameClients,
                      ISource2GameClients, INTERFACEVERSION_SERVERGAMECLIENTS);
  GET_V_IFACE_CURRENT(GetEngineFactory, g_pEngineServer, IVEngineServer2,
                      INTERFACEVERSION_VENGINESERVER);
  GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
  GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkServerService,
                      INetworkServerService,
                      NETWORKSERVERSERVICE_INTERFACE_VERSION);

  g_PlayerManager.Reset();
  m_lastReportTime = 0.0;
  m_serverActive = false;

  g_Config.Load();

  if (g_Config.apiUrl[0] == '\0') {
    META_CONPRINTF("[FKZ] No api_url configured, reporting disabled\n");
  } else {
    META_CONPRINTF("[FKZ] v%s loaded - reporting to %s every %.0fs (key=%s)\n",
                   GetVersion(), g_Config.apiUrl, g_Config.interval,
                   g_Config.apiKey[0] != '\0' ? "set" : "NOT SET");
  }

  // Register hooks
  SH_ADD_HOOK_MEMFUNC(ISource2Server, GameFrame, g_pSource2Server, this,
                      &MMSPlugin::Hook_GameFrame, true);
  SH_ADD_HOOK_MEMFUNC(ISource2Server, ServerHibernationUpdate, g_pSource2Server,
                      this, &MMSPlugin::Hook_ServerHibernationUpdate, true);
  SH_ADD_HOOK_MEMFUNC(ISource2GameClients, ClientPutInServer,
                      g_pSource2GameClients, this,
                      &MMSPlugin::Hook_ClientPutInServer, true);
  SH_ADD_HOOK_MEMFUNC(ISource2GameClients, ClientDisconnect,
                      g_pSource2GameClients, this,
                      &MMSPlugin::Hook_ClientDisconnect, true);
  SH_ADD_HOOK_MEMFUNC(ISource2GameClients, OnClientConnected,
                      g_pSource2GameClients, this,
                      &MMSPlugin::Hook_OnClientConnected, true);
  SH_ADD_HOOK_MEMFUNC(INetworkServerService, StartupServer,
                      g_pNetworkServerService, this,
                      &MMSPlugin::Hook_StartupServer, true);

  // Late load: server is already running
  if (late) {
    g_pGlobals = ismm->GetCGlobals();
    g_HttpClient.Init();
    m_serverActive = true;
    g_ServerInfo.Cache();
    m_lastReportTime = Plat_FloatTime();
  }

  return true;
}

bool MMSPlugin::Unload(char *error, size_t maxlen) {
  SH_REMOVE_HOOK_MEMFUNC(ISource2Server, GameFrame, g_pSource2Server, this,
                         &MMSPlugin::Hook_GameFrame, true);
  SH_REMOVE_HOOK_MEMFUNC(ISource2Server, ServerHibernationUpdate,
                         g_pSource2Server, this,
                         &MMSPlugin::Hook_ServerHibernationUpdate, true);
  SH_REMOVE_HOOK_MEMFUNC(ISource2GameClients, ClientPutInServer,
                         g_pSource2GameClients, this,
                         &MMSPlugin::Hook_ClientPutInServer, true);
  SH_REMOVE_HOOK_MEMFUNC(ISource2GameClients, ClientDisconnect,
                         g_pSource2GameClients, this,
                         &MMSPlugin::Hook_ClientDisconnect, true);
  SH_REMOVE_HOOK_MEMFUNC(ISource2GameClients, OnClientConnected,
                         g_pSource2GameClients, this,
                         &MMSPlugin::Hook_OnClientConnected, true);
  SH_REMOVE_HOOK_MEMFUNC(INetworkServerService, StartupServer,
                         g_pNetworkServerService, this,
                         &MMSPlugin::Hook_StartupServer, true);

  g_HttpClient.ReleasePending();

  return true;
}

void MMSPlugin::AllPluginsLoaded() { g_HttpClient.Init(); }

void *MMSPlugin::OnMetamodQuery(const char *iface, int *ret) {
  if (iface && strcmp(iface, FKZ_API_INTERFACE) == 0) {
    if (ret)
      *ret = META_IFACE_OK;
    return static_cast<void *>(static_cast<IFKZApi *>(&g_FKZApi));
  }

  if (ret)
    *ret = META_IFACE_FAILED;
  return NULL;
}

// Hooks

void MMSPlugin::Hook_StartupServer(const GameSessionConfiguration_t &config,
                                   ISource2WorldSession *, const char *) {
  g_pGlobals = g_SMAPI->GetCGlobals();
  m_serverActive = true;

  g_HttpClient.Init();
  g_ServerInfo.Cache();

  // Reset report timer, first report after short delay
  m_lastReportTime = Plat_FloatTime() + 2.0 - g_Config.interval;

  META_CONPRINTF("[FKZ] Server started, reporting active\n");
}

void MMSPlugin::Hook_OnClientConnected(CPlayerSlot slot, const char *pszName,
                                       uint64 xuid, const char *pszNetworkID,
                                       const char *pszAddress,
                                       bool bFakePlayer) {
  g_PlayerManager.OnClientConnected(slot.Get(), pszName, xuid, pszAddress,
                                    bFakePlayer);
}

void MMSPlugin::Hook_ClientPutInServer(CPlayerSlot slot, char const *pszName,
                                       int type, uint64 xuid) {
  g_PlayerManager.OnClientPutInServer(slot.Get(), pszName, type, xuid);
}

void MMSPlugin::Hook_ClientDisconnect(CPlayerSlot slot,
                                      ENetworkDisconnectionReason reason,
                                      const char *pszName, uint64 xuid,
                                      const char *pszNetworkID) {
  int s = slot.Get();
  bool wasFakeClient = g_PlayerManager.GetPlayer(s).isBot;

  g_PlayerManager.OnClientDisconnect(s);

  // Send hibernate signal when last human player leaves
  if (!wasFakeClient && g_PlayerManager.GetHumanPlayerCount() == 0 &&
      g_Config.apiUrl[0] != '\0')
    SendHibernate();
}

void MMSPlugin::Hook_ServerHibernationUpdate(bool bHibernating) {
  if (bHibernating && g_Config.apiUrl[0] != '\0')
    SendHibernate();
}

void MMSPlugin::Hook_GameFrame(bool simulating, bool bFirstTick,
                               bool bLastTick) {
  if (!m_serverActive)
    return;

  if (!g_HttpClient.IsReady())
    g_HttpClient.Init();

  // Pump Steam callbacks so HTTP completion handlers fire,
  // both the status updater's and any responses for the public API consumers.
  g_HttpClient.RunCallbacks();

  if (g_Config.apiUrl[0] == '\0')
    return;

  double now = Plat_FloatTime();
  if (now - m_lastReportTime >= g_Config.interval) {
    m_lastReportTime = now;
    SendReport();
  }
}
