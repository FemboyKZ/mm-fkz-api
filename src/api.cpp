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

		char url[1024];
		snprintf(url, sizeof(url), "%s%s%s", g_Config.apiUrl, (path[0] == '/') ? "" : "/", path);

		// Give the request at least the report interval before timing out
		uint32 timeout = (uint32)(g_Config.interval > 10.0f ? g_Config.interval : 10.0f);

		ApiCallCtx *ctx = new ApiCallCtx {cb, data};
		if (!g_HttpClient.Request(method, url, body, timeout, ApiTrampoline, ctx))
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
			snprintf(buf, sizeof(buf), "%ssort=%s", sep, sort);
			p += buf;
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
	char path[128];
	snprintf(path, sizeof(path), "/servers/%s", ip);
	return Dispatch("GET", path, nullptr, callback, data);
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
	char path[96];
	snprintf(path, sizeof(path), "/players/%s", steamid);
	return Dispatch("GET", path, nullptr, callback, data);
}

bool FKZApi::GetMaps(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/maps", callback, limit, offset, sort, data);
}

bool FKZApi::GetMap(const char *mapname, FKZ_ResponseCallback callback, void *data)
{
	char path[160];
	snprintf(path, sizeof(path), "/maps/%s", mapname);
	return Dispatch("GET", path, nullptr, callback, data);
}

// KZ Global - records
bool FKZApi::GetKzRecords(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/kzglobal/records", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzRecentRecords(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/kzglobal/records/recent", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzWorldRecords(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/kzglobal/records/worldrecords", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzLeaderboard(const char *mapname, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	char base[192];
	snprintf(base, sizeof(base), "/kzglobal/records/leaderboard/%s", mapname);
	return GetCollection(base, callback, limit, offset, sort, data);
}

bool FKZApi::GetKzRecord(int id, FKZ_ResponseCallback callback, void *data)
{
	char path[96];
	snprintf(path, sizeof(path), "/kzglobal/records/%d", id);
	return Dispatch("GET", path, nullptr, callback, data);
}

// KZ Global - players
bool FKZApi::GetKzPlayers(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/kzglobal/players", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzPlayer(const char *steamid, FKZ_ResponseCallback callback, void *data)
{
	char path[96];
	snprintf(path, sizeof(path), "/kzglobal/players/%s", steamid);
	return Dispatch("GET", path, nullptr, callback, data);
}

bool FKZApi::GetKzPlayerRecords(const char *steamid, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	char base[128];
	snprintf(base, sizeof(base), "/kzglobal/players/%s/records", steamid);
	return GetCollection(base, callback, limit, offset, sort, data);
}

bool FKZApi::GetKzPlayerPBs(const char *steamid, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	char base[128];
	snprintf(base, sizeof(base), "/kzglobal/players/%s/pbs", steamid);
	return GetCollection(base, callback, limit, offset, sort, data);
}

bool FKZApi::GetKzPlayerCompletions(const char *steamid, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	char base[128];
	snprintf(base, sizeof(base), "/kzglobal/players/%s/completions", steamid);
	return GetCollection(base, callback, limit, offset, sort, data);
}

// KZ Global - maps
bool FKZApi::GetKzMaps(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/kzglobal/maps", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzMap(const char *mapname, FKZ_ResponseCallback callback, void *data)
{
	char path[192];
	snprintf(path, sizeof(path), "/kzglobal/maps/%s", mapname);
	return Dispatch("GET", path, nullptr, callback, data);
}

bool FKZApi::GetKzMapRecords(const char *mapname, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	char base[192];
	snprintf(base, sizeof(base), "/kzglobal/maps/%s/records", mapname);
	return GetCollection(base, callback, limit, offset, sort, data);
}

bool FKZApi::GetKzMapCourses(const char *mapname, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	char base[192];
	snprintf(base, sizeof(base), "/kzglobal/maps/%s/courses", mapname);
	return GetCollection(base, callback, limit, offset, sort, data);
}

// KZ Global - servers
bool FKZApi::GetKzServers(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/kzglobal/servers", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzServer(int id, FKZ_ResponseCallback callback, void *data)
{
	char path[96];
	snprintf(path, sizeof(path), "/kzglobal/servers/%d", id);
	return Dispatch("GET", path, nullptr, callback, data);
}

// KZ Global - bans
bool FKZApi::GetKzBans(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/kzglobal/bans", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzActiveBans(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/kzglobal/bans/active", callback, limit, offset, sort, data);
}

bool FKZApi::GetKzBan(int id, FKZ_ResponseCallback callback, void *data)
{
	char path[96];
	snprintf(path, sizeof(path), "/kzglobal/bans/%d", id);
	return Dispatch("GET", path, nullptr, callback, data);
}

bool FKZApi::GetKzPlayerBans(const char *steamid, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	char base[96];
	snprintf(base, sizeof(base), "/kzglobal/bans/player/%s", steamid);
	return GetCollection(base, callback, limit, offset, sort, data);
}

// KZ Local (CS:GO 128/64 tick)
bool FKZApi::GetLocalMaps(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/kzlocal/maps", callback, limit, offset, sort, data);
}

bool FKZApi::GetLocalMap(const char *mapname, FKZ_ResponseCallback callback, void *data)
{
	char path[160];
	snprintf(path, sizeof(path), "/kzlocal/maps/%s", mapname);
	return Dispatch("GET", path, nullptr, callback, data);
}

bool FKZApi::GetLocalRecords(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/kzlocal/records", callback, limit, offset, sort, data);
}

bool FKZApi::GetLocalPlayers(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/kzlocal/players", callback, limit, offset, sort, data);
}

// KZ Local CS2
bool FKZApi::GetLocalCS2Maps(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/kzlocal-cs2/maps", callback, limit, offset, sort, data);
}

bool FKZApi::GetLocalCS2Records(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/kzlocal-cs2/records", callback, limit, offset, sort, data);
}

bool FKZApi::GetLocalCS2Players(FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data)
{
	return GetCollection("/kzlocal-cs2/players", callback, limit, offset, sort, data);
}

bool FKZApi::GetLocalCS2Stats(FKZ_ResponseCallback callback, void *data)
{
	return Dispatch("GET", "/kzlocal-cs2/stats", nullptr, callback, data);
}
