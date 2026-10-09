#ifndef _INCLUDE_DATABASE_H_
#define _INCLUDE_DATABASE_H_

// One shared connection, through mm-utils' sql_mm wrapper, for local per-player preferences. All work is async.

namespace mmu
{
	namespace sql
	{
		class Connection;
	}
} // namespace mmu

// Opens the connection from g_Config and creates the prefs schema.
// No-op when db_driver is unset or sql_mm is absent.
// Call once all plugins are loaded (AllPluginsLoaded).
void Database_Init();

// Destroys the connection for good, at unload. Safe to call when nothing was opened.
void Database_Cleanup();

// Tries a failed connect or schema again. Call once per game frame.
void Database_RunFrame();

// True once connected and the schema exists.
bool Database_IsReady();

mmu::sql::Connection &Database_GetConnection();

// The prefs table with db_prefix in front.
const char *Database_PrefsTable();

#endif // _INCLUDE_DATABASE_H_
