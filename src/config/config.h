#ifndef _INCLUDE_FKZ_CONFIG_H_
#define _INCLUDE_FKZ_CONFIG_H_

#include "utils/config_blocks.h"

#include <string>

struct FKZConfig
{
	std::string apiUrl;
	std::string apiKey;
	std::string serverIp;
	int serverPort = 0;
	float interval = 10.0f;

	// Local per-player database (via the sql_mm plugin).
	bool databaseEnabled = false;
	mmu::config::DatabaseBlock database =
		mmu::config::DatabaseBlock::Defaults("", "addons/fkz-api/data/fkz-api.db", "", "addons/fkz-api/data/prefs.sqlite3");

	mmu::config::GeneralBlock general;

	mmu::config::LogBlock log;
};

// Load and parse a core.cfg. A file that can't be read leaves `config` as it was.
bool FKZ_LoadConfig(const char *path, FKZConfig &config);

extern FKZConfig g_FKZConfig;

#endif // _INCLUDE_FKZ_CONFIG_H_
