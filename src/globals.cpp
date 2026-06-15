#include "globals.h"

// g_pSource2Server, g_pSource2GameClients, g_pEngineServer, g_pCVar,
// g_pNetworkServerService are provided by interfaces.lib from HL2SDK.
// Only define symbols not in interfaces.lib:
CGlobalVars *g_pGlobals = nullptr;
IGameEventSystem *g_pGameEventSystem = nullptr;
