/**
 * FKZ API - Local database
 *
 * See database.h.
 */

#include <vector>

#include "database.h"
#include "config.h"
#include "cross_chat.h"
#include "player_manager.h"
#include "plugin.h"

#include <tier0/platform.h>
#include <tier1/strtools.h>

#include "interfaces/sql_mm/sql_mm.h"
#include "interfaces/sql_mm/sqlite_mm.h"
#include "interfaces/sql_mm/mysql_mm.h"

static ISQLConnection *g_dbConnection = nullptr;
static DatabaseType g_dbType = DatabaseType::None;
static bool g_dbReady = false;
static bool g_dbDestroyPending = false;
// When a failed connect is tried again, 0 while none is owed.
static double g_dbRetryTime = 0.0;
static char g_prefsTable[64];

#define DB_RETRY_SECONDS 30.0

// One row per player, keyed by steamID64.
#define CREATE_PREFS_SQLITE "CREATE TABLE IF NOT EXISTS %s (steamid INTEGER PRIMARY KEY, crosschat_muted INTEGER NOT NULL DEFAULT 0);"
#define CREATE_PREFS_MYSQL  "CREATE TABLE IF NOT EXISTS %s (steamid BIGINT UNSIGNED PRIMARY KEY, crosschat_muted TINYINT NOT NULL DEFAULT 0);"

bool Database_IsReady()
{
	return g_dbReady && g_dbConnection != nullptr && !g_dbDestroyPending;
}

ISQLConnection *Database_GetConnection()
{
	return g_dbConnection;
}

DatabaseType Database_GetType()
{
	return g_dbType;
}

const char *Database_PrefsTable()
{
	return g_prefsTable;
}

static void OnMigrationDone(std::vector<ISQLQuery *> /*queries*/)
{
	g_dbReady = true;
	META_CONPRINTF("[FKZ] Local database ready.\n");

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

static void OnMigrationFail(std::string error, int failIndex)
{
	META_CONPRINTF("[FKZ] Database migration failed at %d: %s\n", failIndex, error.c_str());
}

static void OnConnected(bool success)
{
	if (!success)
	{
		META_CONPRINTF("[FKZ] Database connection failed, trying again in %.0fs.\n", DB_RETRY_SECONDS);
		// Destroy() erases the connection from the vector sql_mm is iterating to reach this callback, so it waits for the next frame.
		g_dbDestroyPending = true;
		return;
	}

	char create[256];
	snprintf(create, sizeof(create), g_dbType == DatabaseType::MySQL ? CREATE_PREFS_MYSQL : CREATE_PREFS_SQLITE, g_prefsTable);

	Transaction txn;
	txn.queries.push_back(create);
	g_dbConnection->ExecuteTransaction(txn, OnMigrationDone, OnMigrationFail);
}

void Database_Init()
{
	if (g_Config.dbDriver[0] == '\0')
	{
		return;
	}
	snprintf(g_prefsTable, sizeof(g_prefsTable), "%splayer_prefs", g_Config.dbPrefix);

	ISQLInterface *sqlInterface = (ISQLInterface *)g_SMAPI->MetaFactory(SQLMM_INTERFACE, nullptr, nullptr);
	if (!sqlInterface)
	{
		META_CONPRINTF("[FKZ] sql_mm plugin not found, local database disabled.\n");
		return;
	}

	if (V_stricmp(g_Config.dbDriver, "sqlite") == 0)
	{
		SQLiteConnectionInfo info;
		// sql_mm resolves this relative to the game dir and creates missing dirs.
		info.database = g_Config.dbDatabase;
		g_dbConnection = sqlInterface->GetSQLiteClient()->CreateSQLiteConnection(info);
		g_dbType = DatabaseType::SQLite;
	}
	else if (V_stricmp(g_Config.dbDriver, "mysql") == 0)
	{
		MySQLConnectionInfo info = {g_Config.dbHost, g_Config.dbUser, g_Config.dbPass, g_Config.dbDatabase, g_Config.dbPort, 60};
		g_dbConnection = sqlInterface->GetMySQLClient()->CreateMySQLConnection(info);
		g_dbType = DatabaseType::MySQL;
	}
	else
	{
		META_CONPRINTF("[FKZ] Unknown db_driver '%s', local database disabled.\n", g_Config.dbDriver);
		return;
	}

	if (!g_dbConnection)
	{
		META_CONPRINTF("[FKZ] Failed to create database connection.\n");
		g_dbType = DatabaseType::None;
		return;
	}

	g_dbConnection->Connect(OnConnected);
}

void Database_RunFrame()
{
	if (g_dbDestroyPending)
	{
		g_dbDestroyPending = false;
		Database_Cleanup();
		g_dbRetryTime = Plat_FloatTime() + DB_RETRY_SECONDS;
	}
	else if (g_dbRetryTime > 0.0 && Plat_FloatTime() >= g_dbRetryTime)
	{
		g_dbRetryTime = 0.0;
		Database_Init();
	}
}

void Database_Cleanup()
{
	g_dbReady = false;
	if (g_dbConnection)
	{
		g_dbConnection->Destroy();
		g_dbConnection = nullptr;
	}
	g_dbType = DatabaseType::None;
}
