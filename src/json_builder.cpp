#include <cstdio>
#include <cstring>

#include "config.h"
#include "cs2kz.h"
#include "globals.h"
#include "json_builder.h"
#include "player_manager.h"
#include "plugin.h"
#include "server_info.h"
#include <ISmmAPI.h>
#include <tier0/platform.h>
#include <tier1/convar.h>

std::string JsonEscape(const char *str) {
  if (!str)
    return "";
  std::string out;
  out.reserve(strlen(str) + 16);
  for (const char *p = str; *p; p++) {
    switch (*p) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\b':
      out += "\\b";
      break;
    case '\f':
      out += "\\f";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(*p) < 0x20) {
        char buf[8];
        snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)*p);
        out += buf;
      } else {
        out += *p;
      }
    }
  }
  return out;
}

// Enumerates loaded Metamod plugins for the report's plugins[] array.
// ISmmPluginManager has no iterator, so we Query sequential ids and stop after
// a run of misses. Query returns false (silently) for unknown ids.
static std::string BuildPluginsJson() {
  int ret = META_IFACE_FAILED;
  ISmmPluginManager *plmgr =
      (ISmmPluginManager *)g_SMAPI->MetaFactory(MMIFACE_PLMANAGER, &ret, NULL);
  if (!plmgr || ret != META_IFACE_OK)
    return "[]";

  std::string arr = "[";
  bool first = true;
  int misses = 0;

  for (PluginId id = Pl_MinId; id <= 256 && misses < 16; id++) {
    const char *file = NULL;
    Pl_Status status = Pl_NotFound;
    PluginId source = 0;

    if (!plmgr->Query(id, &file, &status, &source)) {
      misses++;
      continue;
    }
    misses = 0;

    if (status != Pl_Running && status != Pl_Paused)
      continue;

    const char *name = file ? file : "";
    const char *fwd = strrchr(name, '/');
    const char *bwd = strrchr(name, '\\');
    const char *base = fwd > bwd ? fwd : bwd;
    if (base)
      name = base + 1;

    if (!first)
      arr += ",";
    first = false;

    arr += "{\"name\":\"" + JsonEscape(name) + "\",";
    arr += "\"file\":\"" + JsonEscape(file ? file : "") + "\",";
    arr += std::string("\"status\":\"") +
           (status == Pl_Running ? "running" : "paused") + "\"}";
  }

  arr += "]";
  return arr;
}

// Resolves IP from config or hostip convar
static void ResolveIpPort(char *ip, int ipLen, int &port) {
  if (g_Config.serverIp[0] != '\0') {
    strncpy(ip, g_Config.serverIp, ipLen - 1);
    ip[ipLen - 1] = '\0';
  } else {
    ConVarRefAbstract cvHostIp("hostip");
    if (cvHostIp.IsValidRef()) {
      int hostip = cvHostIp.GetInt();
      snprintf(ip, ipLen, "%d.%d.%d.%d", (hostip >> 24) & 0xFF,
               (hostip >> 16) & 0xFF, (hostip >> 8) & 0xFF, hostip & 0xFF);
    } else {
      strncpy(ip, "0.0.0.0", ipLen);
    }
  }

  port = g_Config.serverPort;
  if (port <= 0) {
    ConVarRefAbstract cvHostPort("hostport");
    if (cvHostPort.IsValidRef())
      port = cvHostPort.GetInt();
    else
      port = 27015;
  }
}

std::string BuildPayloadJson() {
  std::string json;
  json.reserve(4096);

  // Resolve IP and port
  char ip[64];
  int port;
  ResolveIpPort(ip, sizeof(ip), port);

  // Refresh map name each report
  g_ServerInfo.UpdateMap();

  // Player counts
  int playerCount = g_PlayerManager.GetHumanPlayerCount();
  int botCount = g_PlayerManager.GetBotCount();
  int maxPlayers = 0;
  if (g_pNetworkServerService) {
    INetworkGameServer *pGameServer = g_pNetworkServerService->GetIGameServer();
    if (pGameServer)
      maxPlayers = pGameServer->GetMaxClients();
  }
  if (maxPlayers <= 0 && g_pGlobals)
    maxPlayers = g_pGlobals->maxClients;

  // Build server object
  json += "{\"server\":{";
  json += "\"hostname\":\"" + JsonEscape(g_ServerInfo.hostname) + "\",";
  json += "\"os\":\"" + std::string(g_ServerInfo.osName) + "\",";
  json += "\"version\":\"" + JsonEscape(g_ServerInfo.version) + "\",";
  json += "\"tickrate\":" + std::to_string(g_ServerInfo.tickrate) + ",";
  json += std::string("\"secure\":") +
          (g_ServerInfo.secure ? "true" : "false") + ",";
  json += "\"mm_version\":\"" + JsonEscape(g_ServerInfo.mmVersion) + "\",";
  json += "\"cs2kz_loaded\":false,";
  json += "\"plugins\":" + BuildPluginsJson() + ",";
  json += "\"ip\":\"" + JsonEscape(ip) + "\",";
  json += "\"port\":" + std::to_string(port) + ",";
  json += "\"map\":\"" + JsonEscape(g_ServerInfo.mapName) + "\",";
  json += "\"players\":" + std::to_string(playerCount) + ",";
  json += "\"max_players\":" + std::to_string(maxPlayers) + ",";
  json += "\"bot_count\":" + std::to_string(botCount);
  json += "},";

  // Build players array
  json += "\"players\":[";
  bool firstPlayer = true;
  double now = Plat_FloatTime();

  for (int i = 0; i < MAXPLAYERS; i++) {
    const PlayerInfo &player = g_PlayerManager.GetPlayer(i);
    if (!player.connected || !player.inGame || player.isBot)
      continue;

    if (!firstPlayer)
      json += ",";
    firstPlayer = false;

    char steamIdStr[32];
    snprintf(steamIdStr, sizeof(steamIdStr), "%llu",
             (unsigned long long)player.steamId64);

    double timeOnServer = 0.0;
    if (player.connectTime > 0.0)
      timeOnServer = now - player.connectTime;

    json += "{";
    json += "\"steamid\":\"" + std::string(steamIdStr) + "\",";
    json += "\"name\":\"" + JsonEscape(player.name) + "\",";
    json += "\"ip\":\"" + JsonEscape(player.ipAddress) + "\",";

    char timeBuf[32];
    snprintf(timeBuf, sizeof(timeBuf), "%.1f", timeOnServer);
    json += "\"time_on_server\":" + std::string(timeBuf) + ",";

    json +=
        std::string("\"in_game\":") + (player.inGame ? "true" : "false") + ",";
    json += "\"cs2kz\":null,";
    json += "\"playtime_modes\":" + BuildPlaytimeModesJson(i);
    json += "}";
  }

  json += "]}";

  return json;
}

std::string BuildHibernateJson() {
  char ip[64];
  int port;
  ResolveIpPort(ip, sizeof(ip), port);

  std::string json;
  json += "{\"ip\":\"" + JsonEscape(ip) +
          "\",\"port\":" + std::to_string(port) + "}";
  return json;
}
