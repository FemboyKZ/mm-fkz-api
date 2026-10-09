#ifndef _INCLUDE_CONFIG_H_
#define _INCLUDE_CONFIG_H_

#include "utils/config_blocks.h"

#include <string>

struct PluginConfig
{
	std::string apiUrl;
	std::string apiKey;
	std::string serverIp;
	int serverPort = 0;
	float interval = 10.0f;

	// Local per-player database (via the sql_mm plugin).
	bool dbEnabled = false;
	mmu::config::DatabaseBlock db = mmu::config::DatabaseBlock::Defaults("", "addons/fkz-api/data/prefs.sqlite3", "");

	// Each character is an accepted prefix.
	std::string commandPrefix = "!";       // normal (message stays visible)
	std::string silentCommandPrefix = "/"; // silent (message suppressed)

	mmu::config::LogBlock log;

	void Load();
};

extern PluginConfig g_Config;

#endif // _INCLUDE_CONFIG_H_
