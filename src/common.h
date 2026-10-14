#ifndef _INCLUDE_FKZ_COMMON_H_
#define _INCLUDE_FKZ_COMMON_H_

#include <ISmmPlugin.h>
#include <engine2/isource2server.h>
#include <engine2/ivengineserver2.h>
#include <igameevents.h>
#include <engine2/inetworkserverservice.h>
#include <engine2/networkgameserverbase.h>

#define MAXPLAYERS 64

class IGameEventSystem;

extern ISource2Server *g_pSource2Server;
extern ISource2GameClients *g_pSource2GameClients;
extern IVEngineServer2 *g_pEngineServer;
extern ICvar *g_pCVar;
extern CGlobalVars *g_pGlobals;
extern INetworkServerService *g_pNetworkServerService;
extern IGameEventSystem *g_pGameEventSystem;

#endif // _INCLUDE_FKZ_COMMON_H_
