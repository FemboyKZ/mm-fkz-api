#include <cstring>

#include "server_info.h"
#include "globals.h"
#include "plugin.h"
#include <tier1/convar.h>
#include <steam/steam_gameserver.h>

ServerInfo g_ServerInfo;

ServerInfo::ServerInfo()
{
	hostname[0] = '\0';
	mmVersion[0] = '\0';
	mapName[0] = '\0';
	version[0] = '\0';
	tickrate = 64;
	secure = false;

#ifdef _WIN32
	strncpy(osName, "windows", sizeof(osName));
#else
	strncpy(osName, "linux", sizeof(osName));
#endif
}

void ServerInfo::Cache()
{
	// Hostname
	ConVarRefAbstract cvHostname("hostname");
	if (cvHostname.IsValidRef())
	{
		strncpy(hostname, cvHostname.GetString().Get(), sizeof(hostname) - 1);
		hostname[sizeof(hostname) - 1] = '\0';
	}
	else
	{
		strncpy(hostname, "unknown", sizeof(hostname));
	}

	// Metamod version
	int major, minor, plvers, plmin;
	g_SMAPI->GetApiVersions(major, minor, plvers, plmin);
	snprintf(mmVersion, sizeof(mmVersion), "%d.%d", major, minor);

	// VAC status
	secure = SteamGameServer_BSecure();

	// Version from steam.inf
	version[0] = '\0';
	char steamInfPath[512];
	snprintf(steamInfPath, sizeof(steamInfPath), "%s/steam.inf", g_SMAPI->GetBaseDir());
	FILE *f = fopen(steamInfPath, "r");
	if (f)
	{
		char line[256];
		while (fgets(line, sizeof(line), f))
		{
			if (strncmp(line, "PatchVersion=", 13) == 0)
			{
				char *val = line + 13;
				// Trim trailing whitespace/newline
				char *end = val + strlen(val) - 1;
				while (end >= val && (*end == '\n' || *end == '\r' || *end == ' '))
					*end-- = '\0';
				strncpy(version, val, sizeof(version) - 1);
				version[sizeof(version) - 1] = '\0';
				break;
			}
		}
		fclose(f);
	}

	// Map name
	UpdateMap();

	META_CONPRINTF("[FKZ] Cached: hostname=%s, map=%s, version=%s, tickrate=%d, secure=%s\n",
		hostname, mapName, version, tickrate, secure ? "yes" : "no");
}

void ServerInfo::UpdateMap()
{
	// Use INetworkGameServer::GetMapName() - most reliable in CS2
	if (g_pNetworkServerService)
	{
		INetworkGameServer *pGameServer = g_pNetworkServerService->GetIGameServer();
		if (pGameServer)
		{
			const char *name = pGameServer->GetMapName();
			if (name && name[0] != '\0')
			{
				strncpy(mapName, name, sizeof(mapName) - 1);
				mapName[sizeof(mapName) - 1] = '\0';
				return;
			}
		}
	}

	// Fallback: g_pGlobals
	if (g_pGlobals && g_pGlobals->mapname.ToCStr()[0] != '\0')
	{
		strncpy(mapName, g_pGlobals->mapname.ToCStr(), sizeof(mapName) - 1);
		mapName[sizeof(mapName) - 1] = '\0';
	}
}
