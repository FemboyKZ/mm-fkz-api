/**
 * FKZ API - Local database
 *
 * See database.h.
 */

#include "database.h"
#include "config.h"
#include "cross_chat.h"
#include "player_manager.h"
#include "plugin.h"

#include "utils/log.h"
#include "utils/sql.h"

#include <tier0/platform.h>

static mmu::sql::Connection g_db;
static bool g_dbReady = false;
static bool g_schemaCreated = false;
// When a schema that could not be created is tried again, 0 while none is owed.
static double g_schemaRetryTime = 0.0;
static char g_prefsTable[64];

#define DB_RETRY_SECONDS 30.0

// One row per player, keyed by steamID64.
#define CREATE_PREFS_SQLITE "CREATE TABLE IF NOT EXISTS %s (steamid INTEGER PRIMARY KEY, crosschat_muted INTEGER NOT NULL DEFAULT 0);"
#define CREATE_PREFS_MYSQL  "CREATE TABLE IF NOT EXISTS %s (steamid BIGINT UNSIGNED PRIMARY KEY, crosschat_muted TINYINT NOT NULL DEFAULT 0);"

bool Database_IsReady()
{
	return g_dbReady && g_db.IsConnected();
}

mmu::sql::Connection &Database_GetConnection()
{
	return g_db;
}

const char *Database_PrefsTable()
{
	return g_prefsTable;
}

static void OnSchemaSettled(ISQLQuery * /*q*/)
{
	if (!g_schemaCreated)
	{
		MMU_LOG_WARN("Could not create the table %s, trying again in %.0fs.\n", g_prefsTable, DB_RETRY_SECONDS);
		g_schemaRetryTime = Plat_FloatTime() + DB_RETRY_SECONDS;
		return;
	}

	g_dbReady = true;
	MMU_LOG_INFO("Local database ready.\n");

	// Anyone put in server before now, on a late load or ahead of a slow connect, had their prefs load skipped.
	for (int slot = 0; slot < MAXPLAYERS; slot++)
	{
		const PlayerInfo &p = g_PlayerManager.GetPlayer(slot);
		if (p.connected && p.inGame && !p.isBot)
		{
			CrossChat_LoadPrefs(slot, p.steamId64);
		}
	}
}

static void CreateSchema()
{
	char create[256];
	snprintf(create, sizeof(create), g_db.IsMySQL() ? CREATE_PREFS_MYSQL : CREATE_PREFS_SQLITE, g_prefsTable);

	g_schemaCreated = false;
	g_db.Query(create, [](ISQLQuery *q) { g_schemaCreated = q != nullptr; });
	// A query that failed is only heard of once a later one is answered.
	g_db.Query("SELECT 1", OnSchemaSettled);
}

void Database_Init()
{
	if (!g_Config.dbEnabled || !g_db.Init(g_Config.db.DbType()))
	{
		return;
	}
	snprintf(g_prefsTable, sizeof(g_prefsTable), "%splayer_prefs", g_Config.db.prefix.c_str());
	g_db.SetSchemaHook(CreateSchema);
	g_db.Connect(g_Config.db.ToConnectParams(), nullptr);
}

void Database_RunFrame()
{
	double now = Plat_FloatTime();
	g_db.RunFrame(now);

	if (g_schemaRetryTime > 0.0 && now >= g_schemaRetryTime)
	{
		g_schemaRetryTime = 0.0;
		CreateSchema();
	}
}

void Database_Cleanup()
{
	g_dbReady = false;
	g_schemaRetryTime = 0.0;
	g_db.Shutdown();
}
