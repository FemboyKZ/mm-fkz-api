#include <cstdlib>

#include "config.h"
#include "version_gen.h"
#include "plugin.h"

#include "utils/kv_parser.h"
#include "utils/log.h"
#include "utils/str.h"

PluginConfig g_Config;

static void ConfigHandler(const std::string &section, const std::string &key, const std::string &value, void *userdata)
{
	PluginConfig *cfg = static_cast<PluginConfig *>(userdata);
	std::string sec = str::ToLower(section);
	std::string k = str::ToLower(key);

	if (sec == "config")
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
		else if (k == "commandprefix")
		{
			cfg->commandPrefix = value;
		}
		else if (k == "silentcommandprefix")
		{
			cfg->silentCommandPrefix = value;
		}
		else
		{
			mmu::config::ApplyLogKey(cfg->log, k, value);
		}
	}
	else if (sec == "database")
	{
		if (k == "enabled")
		{
			cfg->dbEnabled = (value != "0");
		}
		else
		{
			mmu::config::ApplyDatabaseKey(cfg->db, k, value);
		}
	}
}

void PluginConfig::Load()
{
	*this = PluginConfig();

	const std::string base = g_SMAPI->GetBaseDir();
	const std::string path = base + "/cfg/" PLUGIN_NAME "/core.cfg";
	// Fallback: alongside the plugin's addons folder.
	if (!kv::LoadFile(path, ConfigHandler, this) && !kv::LoadFile(base + "/addons/" PLUGIN_NAME "/core.cfg", ConfigHandler, this))
	{
		MMU_LOG_WARN("No config read from %s. It has to be \"" PLUGIN_NAME "\" { ... } with its sections, "
					 "the key \"value\" lines of older versions are not read.\n",
					 path.c_str());
	}

	mmu::config::ApplyLogBlock(log);

	if (interval < 1.0f)
	{
		interval = 1.0f;
	}
	if (!apiUrl.empty() && apiUrl.back() == '/')
	{
		apiUrl.pop_back();
	}
}
