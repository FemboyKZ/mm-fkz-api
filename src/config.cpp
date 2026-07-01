#include <stdio.h>
#include <cstring>
#include <cstdlib>

#include "config.h"
#include "version_gen.h"
#include "plugin.h"

PluginConfig g_Config;

PluginConfig::PluginConfig()
{
	apiUrl[0] = '\0';
	apiKey[0] = '\0';
	serverIp[0] = '\0';
	serverPort = 0;
	interval = 10.0f;
	dbDriver[0] = '\0';
	dbDatabase[0] = '\0';
	dbHost[0] = '\0';
	dbUser[0] = '\0';
	dbPass[0] = '\0';
	dbPort = 3306;
}

// Config file parser: key "value" format
static bool ParseConfigLine(const char *line, char *key, int keyLen, char *value, int valueLen)
{
	int pos = 0;

	// Skip leading whitespace
	while (line[pos] == ' ' || line[pos] == '\t')
	{
		pos++;
	}

	// Read key (optionally quoted)
	if (line[pos] == '"')
	{
		pos++;
		int start = pos;
		while (line[pos] != '"' && line[pos] != '\0')
		{
			pos++;
		}
		int len = pos - start;
		if (len >= keyLen)
		{
			len = keyLen - 1;
		}
		strncpy(key, line + start, len);
		key[len] = '\0';
		if (line[pos] == '"')
		{
			pos++;
		}
	}
	else
	{
		int start = pos;
		while (line[pos] != ' ' && line[pos] != '\t' && line[pos] != '\0')
		{
			pos++;
		}
		int len = pos - start;
		if (len >= keyLen)
		{
			len = keyLen - 1;
		}
		strncpy(key, line + start, len);
		key[len] = '\0';
	}

	// Skip whitespace between key and value
	while (line[pos] == ' ' || line[pos] == '\t')
	{
		pos++;
	}

	// Read value (must be quoted)
	if (line[pos] == '"')
	{
		pos++;
		int start = pos;
		while (line[pos] != '"' && line[pos] != '\0')
		{
			pos++;
		}
		int len = pos - start;
		if (len >= valueLen)
		{
			len = valueLen - 1;
		}
		strncpy(value, line + start, len);
		value[len] = '\0';
		return true;
	}

	return false;
}

void PluginConfig::Load()
{
	apiUrl[0] = '\0';
	apiKey[0] = '\0';
	serverIp[0] = '\0';
	serverPort = 0;
	interval = 10.0f;
	dbDriver[0] = '\0';
	dbDatabase[0] = '\0';
	dbHost[0] = '\0';
	dbUser[0] = '\0';
	dbPass[0] = '\0';
	dbPort = 3306;

	// Build absolute path using Metamod's game base directory
	const char *baseDir = g_SMAPI->GetBaseDir();
	char cfgPath[512];
	snprintf(cfgPath, sizeof(cfgPath), "%s/cfg/%s/core.cfg", baseDir, PLUGIN_NAME);

	FILE *file = fopen(cfgPath, "r");
	if (!file)
	{
		// Fallback: alongside the plugin's addons folder.
		snprintf(cfgPath, sizeof(cfgPath), "%s/addons/%s/core.cfg", baseDir, PLUGIN_NAME);
		file = fopen(cfgPath, "r");
	}
	if (!file)
	{
		META_CONPRINTF("[FKZ] Config not found at %s/cfg/%s/core.cfg\n", baseDir, PLUGIN_NAME);
		return;
	}

	char line[512];
	while (fgets(line, sizeof(line), file))
	{
		// Trim newline
		size_t len = strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
		{
			line[--len] = '\0';
		}

		// Skip empty lines and comments
		const char *trimmed = line;
		while (*trimmed == ' ' || *trimmed == '\t')
		{
			trimmed++;
		}
		if (*trimmed == '\0' || *trimmed == '/' || *trimmed == '#')
		{
			continue;
		}

		char key[64], value[256];
		if (ParseConfigLine(trimmed, key, sizeof(key), value, sizeof(value)))
		{
			if (strcmp(key, "api_url") == 0)
			{
				strncpy(apiUrl, value, sizeof(apiUrl) - 1);
			}
			else if (strcmp(key, "api_key") == 0)
			{
				strncpy(apiKey, value, sizeof(apiKey) - 1);
			}
			else if (strcmp(key, "server_ip") == 0)
			{
				strncpy(serverIp, value, sizeof(serverIp) - 1);
			}
			else if (strcmp(key, "server_port") == 0)
			{
				serverPort = atoi(value);
			}
			else if (strcmp(key, "interval") == 0)
			{
				interval = static_cast<float>(atof(value));
				if (interval < 1.0f)
				{
					interval = 1.0f;
				}
			}
			else if (strcmp(key, "db_driver") == 0)
			{
				strncpy(dbDriver, value, sizeof(dbDriver) - 1);
			}
			else if (strcmp(key, "db_database") == 0)
			{
				strncpy(dbDatabase, value, sizeof(dbDatabase) - 1);
			}
			else if (strcmp(key, "db_host") == 0)
			{
				strncpy(dbHost, value, sizeof(dbHost) - 1);
			}
			else if (strcmp(key, "db_user") == 0)
			{
				strncpy(dbUser, value, sizeof(dbUser) - 1);
			}
			else if (strcmp(key, "db_pass") == 0)
			{
				strncpy(dbPass, value, sizeof(dbPass) - 1);
			}
			else if (strcmp(key, "db_port") == 0)
			{
				dbPort = atoi(value);
			}
		}
	}

	fclose(file);

	size_t urlLen = strlen(apiUrl);
	if (urlLen > 0 && apiUrl[urlLen - 1] == '/')
	{
		apiUrl[urlLen - 1] = '\0';
	}
}
