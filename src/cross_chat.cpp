/**
 * FKZ API - Cross-server chat (CS2)
 *
 * See cross_chat.h. Capture path hooks the engine's say dispatch. Receive path
 * keeps one long-poll open against the API and prints incoming lines.
 */

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "config.h"
#include "cross_chat.h"
#include "database.h"
#include "globals.h"
#include "http_client.h"
#include "json_builder.h"
#include "player_manager.h"
#include "plugin.h"

#include "../vendor/sql_mm/src/public/sql_mm.h"

#include <engine/igameeventsystem.h>
#include <irecipientfilter.h>
#include <networksystem/inetworkmessages.h>
#include <tier0/platform.h>

#include "thirdparty/nlohmann/json.hpp"
#include "usermessages.pb.h"

// CS2 chat color control bytes (from cs2kz's color table).
#define CHAT_DEFAULT "\x01"
#define CHAT_ORCHID "\x0E"

#define HUD_PRINTTALK 3 // TextMsg destination: the chat area

#define CHAT_RETRY_SECONDS 3.0
#define CHAT_STREAM_TIMEOUT_SECONDS 60 // > the API's ~25s park window

static int g_chatCursor = -1;  // last relay id seen (-1 = needs handshake)
static bool g_streamActive;    // a long-poll request is in flight
static double g_nextRetryTime; // earliest time to (re)open the stream
static bool g_muted[MAXPLAYERS + 1];

// Routes a message to a fixed set of player slots.
class CChatRecipientFilter : public IRecipientFilter {
public:
  NetChannelBufType_t GetNetworkBufType() const override {
    return BUF_RELIABLE;
  }
  bool IsInitMessage() const override { return false; }
  const CPlayerBitVec &GetRecipients() const override { return m_recipients; }
  CPlayerSlot GetPredictedPlayerSlot() const override {
    return CPlayerSlot(-1);
  }

  void AddRecipient(int slot) {
    if (slot >= 0 && slot < ABSOLUTE_PLAYER_LIMIT)
      m_recipients.Set(slot);
  }
  bool Empty() const { return m_recipients.IsAllClear(); }

private:
  CPlayerBitVec m_recipients;
};

// Prints a chat line to everyone in the filter. Uses TextMsg/HUD_PRINTTALK
static void SendChatLine(const CChatRecipientFilter &filter, const char *line) {
  if (!g_pNetworkMessages || !g_pGameEventSystem || filter.Empty())
    return;

  INetworkMessageInternal *netmsg =
      g_pNetworkMessages->FindNetworkMessagePartial("TextMsg");
  if (!netmsg)
    return;

  auto msg = netmsg->AllocateMessage()->ToPB<CUserMessageTextMsg>();
  msg->set_dest(HUD_PRINTTALK);
  msg->add_param(line); // param[0] is the text, control bytes give color
  msg->add_param("");
  msg->add_param("");
  msg->add_param("");
  msg->add_param("");

  g_pGameEventSystem->PostEventAbstract(
      0, false, const_cast<CChatRecipientFilter *>(&filter), netmsg, msg, 0);
  delete msg;
}

static void PrintCrossChat(const char *alias, const char *name,
                           const char *message) {
  CChatRecipientFilter filter;
  for (int slot = 0; slot < MAXPLAYERS; slot++) {
    const PlayerInfo &p = g_PlayerManager.GetPlayer(slot);
    if (p.connected && p.inGame && !p.isBot && !g_muted[slot])
      filter.AddRecipient(slot);
  }

  char line[768]; // fits [alias<=64] name<=64: message<=512 + control bytes
  snprintf(line, sizeof(line), " " CHAT_ORCHID "[%s]" CHAT_DEFAULT " %s: %s",
           alias, name, message);
  SendChatLine(filter, line);
}

static void NotifyMuteState(int slot, bool muted) {
  CChatRecipientFilter filter;
  filter.AddRecipient(slot);
  if (muted)
    SendChatLine(filter,
                 " " CHAT_ORCHID "[CrossChat]" CHAT_DEFAULT
                 " Cross-server messages hidden. Type !crosschat to show.");
  else
    SendChatLine(filter, " " CHAT_ORCHID "[CrossChat]" CHAT_DEFAULT
                         " Cross-server messages shown.");
}

// Persist the current mute state for the player in this slot.
// Upsert keyed by steamID. char[] query binds the non-format Query overload.
static void SavePref(int slot) {
  if (!Database_IsReady())
    return;
  const PlayerInfo &p = g_PlayerManager.GetPlayer(slot);
  if (!p.connected || p.isBot || p.steamId64 == 0)
    return;

  int muted = g_muted[slot] ? 1 : 0;
  char query[256];
  if (Database_GetType() == DatabaseType::MySQL)
    snprintf(query, sizeof(query),
             "INSERT INTO player_prefs (steamid, crosschat_muted) VALUES "
             "(%llu, %d) ON DUPLICATE KEY UPDATE crosschat_muted = %d;",
             (unsigned long long)p.steamId64, muted, muted);
  else
    snprintf(query, sizeof(query),
             "INSERT INTO player_prefs (steamid, crosschat_muted) VALUES "
             "(%llu, %d) ON CONFLICT(steamid) DO UPDATE SET crosschat_muted = "
             "%d;",
             (unsigned long long)p.steamId64, muted, muted);

  Database_GetConnection()->Query(query, [](ISQLQuery * /*q*/) {});
}

void CrossChat_LoadPrefs(int slot, uint64_t steamId64) {
  if (!Database_IsReady() || slot < 0 || slot >= MAXPLAYERS || steamId64 == 0)
    return;

  char query[256];
  snprintf(query, sizeof(query),
           "SELECT crosschat_muted FROM player_prefs WHERE steamid = %llu;",
           (unsigned long long)steamId64);

  Database_GetConnection()->Query(query, [slot, steamId64](ISQLQuery *q) {
    // The slot may have been recycled before this async result arrived.
    const PlayerInfo &p = g_PlayerManager.GetPlayer(slot);
    if (!p.connected || p.steamId64 != steamId64)
      return;
    ISQLResult *result = q->GetResultSet();
    if (result && result->FetchRow())
      g_muted[slot] = result->GetInt(0) != 0;
  });
}

static void OnChatPost(bool /*success*/, int statusCode, const char * /*body*/,
                       uint32 /*len*/, void * /*data*/) {
  if (statusCode != 200 && statusCode != 0)
    META_CONPRINTF("[FKZ] chat POST returned HTTP %d\n", statusCode);
}

void CrossChat_OnDispatchConCommand(ConCommandRef cmd,
                                    const CCommandContext &ctx,
                                    const CCommand &args) {
  if (g_Config.apiUrl[0] == '\0' || !cmd.IsValidRef())
    return;

  // Public chat only; team chat stays local.
  if (V_stricmp(cmd.GetName(), "say") != 0 || args.ArgC() < 2)
    return;

  int slot = ctx.GetPlayerSlot().Get();
  if (slot < 0 || slot >= MAXPLAYERS)
    return;

  const PlayerInfo &p = g_PlayerManager.GetPlayer(slot);
  if (!p.connected || p.isBot)
    return;

  // args.ArgS() is the remainder after "say", usually wrapped in quotes.
  const char *raw = args.ArgS();
  if (!raw)
    return;
  std::string text = raw;
  if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
    text = text.substr(1, text.size() - 2);

  // Replace CS2's zero-width-space quote artifact with a real quote.
  size_t pos = 0;
  while ((pos = text.find("\xE2\x80\x8B", pos)) != std::string::npos)
    text.replace(pos, 3, "\"");

  // Trim leading whitespace.
  size_t begin = text.find_first_not_of(" \t");
  if (begin == std::string::npos)
    return;
  text = text.substr(begin);
  if (text.empty())
    return;

  // Mute toggle.
  if (V_stricmp(text.c_str(), "!crosschat") == 0 ||
      V_stricmp(text.c_str(), "/crosschat") == 0) {
    g_muted[slot] = !g_muted[slot];
    NotifyMuteState(slot, g_muted[slot]);
    SavePref(slot);
    return;
  }

  // Other command triggers are not relayed.
  if (text[0] == '!' || text[0] == '/')
    return;

  char ip[64];
  int port;
  ResolveIpPort(ip, sizeof(ip), port);

  char steamId[32];
  snprintf(steamId, sizeof(steamId), "%llu", (unsigned long long)p.steamId64);

  std::string body = "{\"ip\":\"" + JsonEscape(ip) +
                     "\",\"port\":" + std::to_string(port) + ",\"steamid\":\"" +
                     steamId + "\",\"name\":\"" + JsonEscape(p.name) +
                     "\",\"message\":\"" + JsonEscape(text.c_str()) + "\"}";

  std::string url = std::string(g_Config.apiUrl) + "/chat/messages";
  g_HttpClient.Request("POST", url.c_str(), body.c_str(), 10, OnChatPost,
                       nullptr);
}

static void OnChatStream(bool /*success*/, int statusCode, const char *body,
                         uint32 /*len*/, void * /*data*/) {
  g_streamActive = false;

  if (statusCode != 200) {
    g_nextRetryTime = Plat_FloatTime() + CHAT_RETRY_SECONDS;
    return;
  }

  if (body && body[0] != '\0') {
    nlohmann::json doc = nlohmann::json::parse(body, nullptr, false);
    if (doc.is_object()) {
      // Trust the server's cursor (only one poll is ever in flight, so replies
      // arrive in order). Adopting it unconditionally lets us recover if the
      // API restarted and reset its cursor below ours.
      g_chatCursor = doc.value("cursor", g_chatCursor);

      auto msgs = doc.find("messages");
      if (msgs != doc.end() && msgs->is_array()) {
        for (const auto &m : *msgs) {
          if (!m.is_object())
            continue;
          std::string alias = m.value("alias", std::string());
          std::string name = m.value("name", std::string());
          std::string message = m.value("message", std::string());
          PrintCrossChat(alias.c_str(), name.c_str(), message.c_str());
        }
      }
    }
  }
  // CrossChat_Tick re-opens the poll on the next game frame.
}

static void StartChatStream() {
  char ip[64];
  int port;
  ResolveIpPort(ip, sizeof(ip), port);

  char url[1024];
  snprintf(url, sizeof(url), "%s/chat/stream?after=%d&ip=%s&port=%d",
           g_Config.apiUrl, g_chatCursor, ip, port);

  if (g_HttpClient.Request("GET", url, nullptr, CHAT_STREAM_TIMEOUT_SECONDS,
                           OnChatStream, nullptr))
    g_streamActive = true;
  else
    g_nextRetryTime = Plat_FloatTime() + CHAT_RETRY_SECONDS;
}

void CrossChat_Tick(bool hasHumans) {
  if (g_Config.apiUrl[0] == '\0' || !hasHumans || g_streamActive)
    return;
  if (Plat_FloatTime() < g_nextRetryTime)
    return;
  StartChatStream();
}

void CrossChat_OnClientDisconnect(int slot) {
  if (slot >= 0 && slot <= MAXPLAYERS)
    g_muted[slot] = false;
}
