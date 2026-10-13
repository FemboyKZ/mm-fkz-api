#include <cstdlib>
#include <utility>

#include "config.h"

#include "utils/kv_parser.h"
#include "utils/log.h"
#include "utils/str.h"

FKZConfig g_FKZConfig;

static void ConfigHandler(const std::string &section, const std::string &key, const std::string &value, void *userdata)
{
	FKZConfig *cfg = static_cast<FKZConfig *>(userdata);
	std::string sec = str::ToLower(section);
	std::string k = str::ToLower(key);
	bool known = true;

	// "config" is the old name.
	if (sec == "general" || sec == "config")
	{
		if (k == "apiurl")
		{
			cfg->apiUrl = value;
		}
		else if (k == "apikey")
		{
			cfg->apiKey = value;
		}
		else if (k == "serverip")
		{
			cfg->serverIp = value;
		}
		else if (k == "serverport")
		{
			cfg->serverPort = atoi(value.c_str());
		}
		else if (k == "interval")
		{
			cfg->interval = static_cast<float>(atof(value.c_str()));
		}
		else
		{
			known = mmu::config::ApplyGeneralKey(cfg->general, k, value) || mmu::config::ApplyLogKey(cfg->log, k, value);
		}
	}
	else if (sec == "database")
	{
		known = mmu::config::ApplyOptionalDatabaseKey(cfg->db, cfg->dbEnabled, k, value);
	}
	else
	{
		known = false;
	}
	if (!known)
	{
		mmu::config::WarnUnknownKey(section, key);
	}
}

bool FKZ_LoadConfig(const char *path, FKZConfig &config)
{
	// Fresh, so a removed key reverts to its default.
	FKZConfig fresh;
	if (!kv::LoadFile(path, ConfigHandler, &fresh))
	{
		return false;
	}

	if (fresh.interval < 1.0f)
	{
		fresh.interval = 1.0f;
	}
	if (!fresh.apiUrl.empty() && fresh.apiUrl.back() == '/')
	{
		fresh.apiUrl.pop_back();
	}

	config = std::move(fresh);
	mmu::config::ApplyLogBlock(config.log);
	return true;
}
