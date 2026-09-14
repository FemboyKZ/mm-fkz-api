#ifndef _INCLUDE_CONFIG_H_
#define _INCLUDE_CONFIG_H_

struct PluginConfig
{
	char apiUrl[256];
	char apiKey[256];
	char serverIp[64];
	int serverPort;
	float interval;

	// Local per-player database (via the sql_mm plugin). Empty driver disables it.
	char dbDriver[16];    // "sqlite" or "mysql"
	char dbDatabase[128]; // sqlite: file path relative to game dir. mysql: schema name
	char dbHost[128];
	char dbUser[64];
	char dbPass[128];
	int dbPort;

	// Each character is an accepted prefix.
	char commandPrefix[8];       // normal (message stays visible)
	char silentCommandPrefix[8]; // silent (message suppressed)

	PluginConfig();
	void Load();
};

extern PluginConfig g_Config;

#endif // _INCLUDE_CONFIG_H_
