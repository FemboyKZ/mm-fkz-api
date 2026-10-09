/**
 * FKZ API - Public API implementation
 *
 * Exposes the FKZ REST API to other Metamod plugins (see include/IFKZApi.h).
 *   - ApiRequest()  generic async request to any endpoint/method.
 *   - Get*()        typed convenience wrappers for the main read endpoints.
 *
 * All requests are asynchronous.
 * The result is delivered to an FKZ_ResponseCallback with the HTTP status and the raw response body.
 *
 * Endpoints map to the API root configured as api_url in core.cfg
 * (e.g. https://api.femboykz.com).
 */

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "api.h"
#include "config.h"
#include "http_client.h"
#include "plugin.h"

#include "utils/log.h"

FKZApi g_FKZApi;

namespace
{
	// Packs the caller's (callback, data) so the HTTP layer can deliver the result.
	struct ApiCallCtx
	{
		PluginId owner;
		FKZ_ResponseCallback cb;
		void *data;
	};

	// Every request not answered yet.
	std::vector<ApiCallCtx *> s_calls;

	void ApiTrampoline(bool success, int statusCode, const char *body, uint32 /*bodyLen*/, void *userData)
	{
		ApiCallCtx *ctx = (ApiCallCtx *)userData;
		s_calls.erase(std::remove(s_calls.begin(), s_calls.end(), ctx), s_calls.end());
		if (ctx->cb)
		{
			ctx->cb(success, statusCode, body, ctx->data);
		}
		delete ctx;
	}

	// Percent-encodes one path segment or query value. Unreserved characters (RFC 3986) pass through.
	std::string Encode(const char *value)
	{
		std::string out;
		for (const char *p = value ? value : ""; *p; p++)
		{
			unsigned char c = (unsigned char)*p;
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
			{
				out += (char)c;
			}
			else
			{
				char buf[4];
				snprintf(buf, sizeof(buf), "%%%02X", c);
				out += buf;
			}
		}
		return out;
	}

	// Builds the absolute URL and dispatches the request.
	bool Dispatch(PluginId owner, const char *method, const char *path, const char *body, FKZ_ResponseCallback cb, void *data)
	{
		if (!cb)
		{
			MMU_LOG_WARN("API request: invalid callback\n");
			return false;
		}
		if (g_Config.apiUrl.empty())
		{
			MMU_LOG_WARN("API request: no api_url configured\n");
			return false;
		}

		std::string url = g_Config.apiUrl;
		if (path[0] != '/')
		{
			url += '/';
		}
		url += path;

		// Give the request at least the report interval before timing out
		uint32 timeout = (uint32)(g_Config.interval > 10.0f ? g_Config.interval : 10.0f);

		ApiCallCtx *ctx = new ApiCallCtx {owner, cb, data};
		if (!g_HttpClient.Request(method, url.c_str(), body, timeout, ApiTrampoline, ctx))
		{
			delete ctx;
			return false;
		}
		s_calls.push_back(ctx);
		return true;
	}

	// Appends limit/offset/sort query parameters, skipping any that are unset.
	std::string Paged(const char *base, int limit, int offset, const char *sort)
	{
		std::string p(base);
		const char *sep = (p.find('?') == std::string::npos) ? "?" : "&";
		char buf[128];

		if (limit > 0)
		{
			snprintf(buf, sizeof(buf), "%slimit=%d", sep, limit);
			p += buf;
			sep = "&";
		}
		if (offset > 0)
		{
			snprintf(buf, sizeof(buf), "%soffset=%d", sep, offset);
			p += buf;
			sep = "&";
		}
		if (sort && sort[0] != '\0')
		{
			p += sep;
			p += "sort=" + Encode(sort);
			sep = "&";
		}
		return p;
	}

	// GET a collection endpoint with pagination.
	bool GetCollection(PluginId owner, const char *base, FKZ_ResponseCallback cb, int limit, int offset, const char *sort, void *data)
	{
		return Dispatch(owner, "GET", Paged(base, limit, offset, sort).c_str(), nullptr, cb, data);
	}
} // namespace

void FKZApi::DropOwnedBy(PluginId owner)
{
	for (ApiCallCtx *ctx : s_calls)
	{
		if (ctx->owner == owner)
		{
			ctx->cb = nullptr;
		}
	}
}

// Core
bool FKZApi::ApiRequest(PluginId owner, const char *method, const char *path, const char *body, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch(owner, method, path, body, callback, data);
}

int FKZApi::GetApiBase(char *buffer, int maxlength)
{
	if (buffer && maxlength > 0)
	{
		strncpy(buffer, g_Config.apiUrl.c_str(), maxlength - 1);
		buffer[maxlength - 1] = '\0';
	}
	return (int)g_Config.apiUrl.size();
}

bool FKZApi::GetHealth(PluginId owner, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch(owner, "GET", "/health", nullptr, callback, data);
}

bool FKZApi::PostServerStatus(PluginId owner, const char *body, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch(owner, "POST", "/servers/status", body, callback, data);
}

bool FKZApi::PostHibernate(PluginId owner, const char *body, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch(owner, "POST", "/servers/status/hibernate", body, callback, data);
}

// Live servers / players / maps
bool FKZApi::GetServers(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, "/servers", callback, limit, offset, sort, data);
}

bool FKZApi::GetServer(PluginId owner, const char *ip, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch(owner, "GET", ("/servers/" + Encode(ip)).c_str(), nullptr, callback, data);
}

bool FKZApi::GetPlayers(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, "/players", callback, limit, offset, sort, data);
}

bool FKZApi::GetOnlinePlayers(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, "/players/online", callback, limit, offset, sort, data);
}

bool FKZApi::GetPlayer(PluginId owner, const char *steamid, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch(owner, "GET", ("/players/" + Encode(steamid)).c_str(), nullptr, callback, data);
}

bool FKZApi::GetMaps(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, "/maps", callback, limit, offset, sort, data);
}

bool FKZApi::GetMap(PluginId owner, const char *mapname, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch(owner, "GET", ("/maps/" + Encode(mapname)).c_str(), nullptr, callback, data);
}

// KZ Global - records
bool FKZApi::GetKzRecords(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, "/global/records", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzRecentRecords(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, "/global/records/recent", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzWorldRecords(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, "/global/records/worldrecords", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzLeaderboard(PluginId owner, const char *mapname, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, ("/global/records/leaderboard/" + Encode(mapname)).c_str(), callback, limit, offset, sort, data);
}

bool FKZApi::GetKzRecord(PluginId owner, int id, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch(owner, "GET", ("/global/records/" + std::to_string(id)).c_str(), nullptr, callback, data);
}

// KZ Global - players
bool FKZApi::GetKzPlayers(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, "/global/players", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzPlayer(PluginId owner, const char *steamid, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch(owner, "GET", ("/global/players/" + Encode(steamid)).c_str(), nullptr, callback, data);
}

bool FKZApi::GetKzPlayerRecords(PluginId owner, const char *steamid, FKZ_ResponseCallback callback, int limit, int offset, const char *sort,
								void *data)
{
	return GetCollection(owner, ("/global/players/" + Encode(steamid) + "/records").c_str(), callback, limit, offset, sort, data);
}

bool FKZApi::GetKzPlayerPBs(PluginId owner, const char *steamid, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, ("/global/players/" + Encode(steamid) + "/pbs").c_str(), callback, limit, offset, sort, data);
}

bool FKZApi::GetKzPlayerCompletions(PluginId owner, const char *steamid, FKZ_ResponseCallback callback, int limit, int offset, const char *sort,
									void *data)
{
	return GetCollection(owner, ("/global/players/" + Encode(steamid) + "/completions").c_str(), callback, limit, offset, sort, data);
}

// KZ Global - maps
bool FKZApi::GetKzMaps(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, "/global/maps", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzMap(PluginId owner, const char *mapname, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch(owner, "GET", ("/global/maps/" + Encode(mapname)).c_str(), nullptr, callback, data);
}

bool FKZApi::GetKzMapRecords(PluginId owner, const char *mapname, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, ("/global/maps/" + Encode(mapname) + "/records").c_str(), callback, limit, offset, sort, data);
}

bool FKZApi::GetKzMapCourses(PluginId owner, const char *mapname, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, ("/global/maps/" + Encode(mapname) + "/courses").c_str(), callback, limit, offset, sort, data);
}

// KZ Global - servers
bool FKZApi::GetKzServers(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, "/global/servers", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzServer(PluginId owner, int id, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch(owner, "GET", ("/global/servers/" + std::to_string(id)).c_str(), nullptr, callback, data);
}

// KZ Global - bans
bool FKZApi::GetKzBans(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, "/global/bans", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzActiveBans(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, "/global/bans/active", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzBan(PluginId owner, int id, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch(owner, "GET", ("/global/bans/" + std::to_string(id)).c_str(), nullptr, callback, data);
}

bool FKZApi::GetKzPlayerBans(PluginId owner, const char *steamid, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, ("/global/bans/player/" + Encode(steamid)).c_str(), callback, limit, offset, sort, data);
}

// KZ Local (CS:GO 128/64 tick)
bool FKZApi::GetLocalMaps(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, "/local/gokz/maps", callback, limit, offset, sort, data);
}

bool FKZApi::GetLocalMap(PluginId owner, const char *mapname, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch(owner, "GET", ("/local/gokz/maps/" + Encode(mapname)).c_str(), nullptr, callback, data);
}

bool FKZApi::GetLocalRecords(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, "/local/gokz/records", callback, limit, offset, sort, data);
}

bool FKZApi::GetLocalPlayers(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, "/local/gokz/players", callback, limit, offset, sort, data);
}

// KZ Local CS2
bool FKZApi::GetLocalCS2Maps(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, "/local/cs2kz/maps", callback, limit, offset, sort, data);
}

bool FKZApi::GetLocalCS2Records(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, "/local/cs2kz/records", callback, limit, offset, sort, data);
}

bool FKZApi::GetLocalCS2Players(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(owner, "/local/cs2kz/players", callback, limit, offset, sort, data);
}

bool FKZApi::GetLocalCS2Stats(PluginId owner, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch(owner, "GET", "/local/cs2kz/stats", nullptr, callback, data);
}
