/**
 * FKZ API - Local database
 *
 * See database.h.
 */

#include <vector>

#include "database.h"
#include "config.h"
#include "plugin.h"

#include <tier1/strtools.h>

#include "../vendor/sql_mm/src/public/sql_mm.h"
#include "../vendor/sql_mm/src/public/sqlite_mm.h"
#include "../vendor/sql_mm/src/public/mysql_mm.h"

static ISQLConnection *g_dbConnection = nullptr;
static DatabaseType g_dbType = DatabaseType::None;
static bool g_dbReady = false;

// One row per player, keyed by steamID64.
static const char *kCreatePrefsSqlite = "CREATE TABLE IF NOT EXISTS player_prefs ("
										"steamid INTEGER PRIMARY KEY, "
										"crosschat_muted INTEGER NOT NULL DEFAULT 0);";
static const char *kCreatePrefsMySQL = "CREATE TABLE IF NOT EXISTS player_prefs ("
									   "steamid BIGINT UNSIGNED PRIMARY KEY, "
									   "crosschat_muted TINYINT NOT NULL DEFAULT 0);";

bool Database_IsReady()
{
	return g_dbReady && g_dbConnection != nullptr;
}

ISQLConnection *Database_GetConnection()
{
	return g_dbConnection;
}

DatabaseType Database_GetType()
{
	return g_dbType;
}

static void OnMigrationDone(std::vector<ISQLQuery *> /*queries*/)
{
	g_dbReady = true;
	META_CONPRINTF("[FKZ] Local database ready.\n");
}

static void OnMigrationFail(std::string error, int failIndex)
{
	META_CONPRINTF("[FKZ] Database migration failed at %d: %s\n", failIndex, error.c_str());
}

static void OnConnected(bool success)
{
	if (!success)
	{
		META_CONPRINTF("[FKZ] Database connection failed.\n");
		g_dbConnection->Destroy();
		g_dbConnection = nullptr;
		g_dbType = DatabaseType::None;
		return;
	}

	Transaction txn;
	txn.queries.push_back(g_dbType == DatabaseType::MySQL ? kCreatePrefsMySQL : kCreatePrefsSqlite);
	g_dbConnection->ExecuteTransaction(txn, OnMigrationDone, OnMigrationFail);
}

void Database_Init()
{
	if (g_Config.dbDriver[0] == '\0')
	{
		return;
	}

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
