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

#include <cstdio>
#include <cstring>
#include <string>

#include "api.h"
#include "config.h"
#include "http_client.h"
#include "plugin.h"

FKZApi g_FKZApi;

namespace
{
	// Packs the caller's (callback, data) so the HTTP layer can deliver the result.
	struct ApiCallCtx
	{
		FKZ_ResponseCallback cb;
		void *data;
	};

	void ApiTrampoline(bool success, int statusCode, const char *body, uint32 /*bodyLen*/, void *userData)
	{
		ApiCallCtx *ctx = (ApiCallCtx *)userData;
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
	bool Dispatch(const char *method, const char *path, const char *body, FKZ_ResponseCallback cb, void *data)
	{
		if (!cb)
		{
			META_CONPRINTF("[FKZ] API request: invalid callback\n");
			return false;
		}
		if (g_Config.apiUrl[0] == '\0')
		{
			META_CONPRINTF("[FKZ] API request: no api_url configured\n");
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

		ApiCallCtx *ctx = new ApiCallCtx {cb, data};
		if (!g_HttpClient.Request(method, url.c_str(), body, timeout, ApiTrampoline, ctx))
		{
			delete ctx;
			return false;
		}
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
	bool GetCollection(const char *base, FKZ_ResponseCallback cb, int limit, int offset, const char *sort, void *data)
	{
		return Dispatch("GET", Paged(base, limit, offset, sort).c_str(), nullptr, cb, data);
	}
} // namespace

// Core
bool FKZApi::ApiRequest(const char *method, const char *path, const char *body, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch(method, path, body, callback, data);
}

int FKZApi::GetApiBase(char *buffer, int maxlength)
{
	if (buffer && maxlength > 0)
	{
		strncpy(buffer, g_Config.apiUrl, maxlength - 1);
		buffer[maxlength - 1] = '\0';
	}
	return (int)strlen(g_Config.apiUrl);
}

bool FKZApi::GetHealth(FKZ_ResponseCallback callback, void *data)
{
	return Dispatch("GET", "/health", nullptr, callback, data);
}

bool FKZApi::PostServerStatus(const char *body, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch("POST", "/servers/status", body, callback, data);
}

bool FKZApi::PostHibernate(const char *body, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch("POST", "/servers/status/hibernate", body, callback, data);
}

// Live servers / players / maps
bool FKZApi::GetServers(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/servers", callback, limit, offset, sort, data);
}

bool FKZApi::GetServer(const char *ip, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch("GET", ("/servers/" + Encode(ip)).c_str(), nullptr, callback, data);
}

bool FKZApi::GetPlayers(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/players", callback, limit, offset, sort, data);
}

bool FKZApi::GetOnlinePlayers(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/players/online", callback, limit, offset, sort, data);
}

bool FKZApi::GetPlayer(const char *steamid, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch("GET", ("/players/" + Encode(steamid)).c_str(), nullptr, callback, data);
}

bool FKZApi::GetMaps(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/maps", callback, limit, offset, sort, data);
}

bool FKZApi::GetMap(const char *mapname, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch("GET", ("/maps/" + Encode(mapname)).c_str(), nullptr, callback, data);
}

// KZ Global - records
bool FKZApi::GetKzRecords(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/global/records", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzRecentRecords(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/global/records/recent", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzWorldRecords(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/global/records/worldrecords", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzLeaderboard(const char *mapname, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(("/global/records/leaderboard/" + Encode(mapname)).c_str(), callback, limit, offset, sort, data);
}

bool FKZApi::GetKzRecord(int id, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch("GET", ("/global/records/" + std::to_string(id)).c_str(), nullptr, callback, data);
}

// KZ Global - players
bool FKZApi::GetKzPlayers(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/global/players", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzPlayer(const char *steamid, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch("GET", ("/global/players/" + Encode(steamid)).c_str(), nullptr, callback, data);
}

bool FKZApi::GetKzPlayerRecords(const char *steamid, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(("/global/players/" + Encode(steamid) + "/records").c_str(), callback, limit, offset, sort, data);
}

bool FKZApi::GetKzPlayerPBs(const char *steamid, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(("/global/players/" + Encode(steamid) + "/pbs").c_str(), callback, limit, offset, sort, data);
}

bool FKZApi::GetKzPlayerCompletions(const char *steamid, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(("/global/players/" + Encode(steamid) + "/completions").c_str(), callback, limit, offset, sort, data);
}

// KZ Global - maps
bool FKZApi::GetKzMaps(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/global/maps", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzMap(const char *mapname, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch("GET", ("/global/maps/" + Encode(mapname)).c_str(), nullptr, callback, data);
}

bool FKZApi::GetKzMapRecords(const char *mapname, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(("/global/maps/" + Encode(mapname) + "/records").c_str(), callback, limit, offset, sort, data);
}

bool FKZApi::GetKzMapCourses(const char *mapname, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(("/global/maps/" + Encode(mapname) + "/courses").c_str(), callback, limit, offset, sort, data);
}

// KZ Global - servers
bool FKZApi::GetKzServers(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/global/servers", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzServer(int id, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch("GET", ("/global/servers/" + std::to_string(id)).c_str(), nullptr, callback, data);
}

// KZ Global - bans
bool FKZApi::GetKzBans(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/global/bans", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzActiveBans(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/global/bans/active", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzBan(int id, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch("GET", ("/global/bans/" + std::to_string(id)).c_str(), nullptr, callback, data);
}

bool FKZApi::GetKzPlayerBans(const char *steamid, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection(("/global/bans/player/" + Encode(steamid)).c_str(), callback, limit, offset, sort, data);
}

// KZ Local (CS:GO 128/64 tick)
bool FKZApi::GetLocalMaps(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/local/gokz/maps", callback, limit, offset, sort, data);
}

bool FKZApi::GetLocalMap(const char *mapname, FKZ_ResponseCallback callback, void *data)
{
	return Dispatch("GET", ("/local/gokz/maps/" + Encode(mapname)).c_str(), nullptr, callback, data);
}

bool FKZApi::GetLocalRecords(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/local/gokz/records", callback, limit, offset, sort, data);
}

bool FKZApi::GetLocalPlayers(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/local/gokz/players", callback, limit, offset, sort, data);
}

// KZ Local CS2
bool FKZApi::GetLocalCS2Maps(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/local/cs2kz/maps", callback, limit, offset, sort, data);
}

bool FKZApi::GetLocalCS2Records(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/local/cs2kz/records", callback, limit, offset, sort, data);
}

bool FKZApi::GetLocalCS2Players(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/local/cs2kz/players", callback, limit, offset, sort, data);
}

bool FKZApi::GetLocalCS2Stats(FKZ_ResponseCallback callback, void *data)
{
	return Dispatch("GET", "/local/cs2kz/stats", nullptr, callback, data);
}
