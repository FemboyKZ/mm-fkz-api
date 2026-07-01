#ifndef _INCLUDE_DATABASE_H_
#define _INCLUDE_DATABASE_H_

// Thin wrapper around the sql_mm plugin's SQLInterface.
// Provides a single shared connection for local per-player preferences. All work is async.

class ISQLConnection;

enum class DatabaseType
{
	None = -1,
	SQLite,
	MySQL
};

// Acquires SQLInterface from sql_mm, opens the connection from g_Config, and creates the prefs schema.
// No-op when db_driver is unset or sql_mm is absent.
// Call once all plugins are loaded (AllPluginsLoaded).
void Database_Init();

// Destroys the connection. Safe to call when nothing was opened.
void Database_Cleanup();

// True once connected and the schema migration has completed.
bool Database_IsReady();

ISQLConnection *Database_GetConnection();
DatabaseType Database_GetType();

#endif // _INCLUDE_DATABASE_H_
