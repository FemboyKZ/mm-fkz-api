#ifndef _INCLUDE_FKZ_API_H_
#define _INCLUDE_FKZ_API_H_

#include "../include/IFKZApi.h"

// Concrete implementation of the public FKZ API interface.
// Exposed to other Metamod plugins via MMSPlugin::OnMetamodQuery().
class FKZApi : public IFKZApi
{
public:
	int GetInterfaceVersion() override
	{
		return 2;
	}

	// A plugin Metamod has unloaded: its callbacks are code that is gone.
	void DropOwnedBy(PluginId owner);

	// Core
	bool ApiRequest(PluginId owner, const char *method, const char *path, const char *body, FKZ_ResponseCallback callback, void *data) override;
	int GetApiBase(char *buffer, int maxlength) override;
	bool GetHealth(PluginId owner, FKZ_ResponseCallback callback, void *data) override;
	bool PostServerStatus(PluginId owner, const char *body, FKZ_ResponseCallback callback, void *data) override;
	bool PostHibernate(PluginId owner, const char *body, FKZ_ResponseCallback callback, void *data) override;

	// Live servers / players / maps
	bool GetServers(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data) override;
	bool GetServer(PluginId owner, const char *ip, FKZ_ResponseCallback callback, void *data) override;
	bool GetPlayers(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data) override;
	bool GetOnlinePlayers(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data) override;
	bool GetPlayer(PluginId owner, const char *steamid, FKZ_ResponseCallback callback, void *data) override;
	bool GetMaps(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data) override;
	bool GetMap(PluginId owner, const char *mapname, FKZ_ResponseCallback callback, void *data) override;

	// KZ Global - records
	bool GetKzRecords(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data) override;
	bool GetKzRecentRecords(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data) override;
	bool GetKzWorldRecords(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data) override;
	bool GetKzLeaderboard(PluginId owner, const char *mapname, FKZ_ResponseCallback callback, int limit, int offset, const char *sort,
						  void *data) override;
	bool GetKzRecord(PluginId owner, int id, FKZ_ResponseCallback callback, void *data) override;

	// KZ Global - players
	bool GetKzPlayers(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data) override;
	bool GetKzPlayer(PluginId owner, const char *steamid, FKZ_ResponseCallback callback, void *data) override;
	bool GetKzPlayerRecords(PluginId owner, const char *steamid, FKZ_ResponseCallback callback, int limit, int offset, const char *sort,
							void *data) override;
	bool GetKzPlayerPBs(PluginId owner, const char *steamid, FKZ_ResponseCallback callback, int limit, int offset, const char *sort,
						void *data) override;
	bool GetKzPlayerCompletions(PluginId owner, const char *steamid, FKZ_ResponseCallback callback, int limit, int offset, const char *sort,
								void *data) override;

	// KZ Global - maps
	bool GetKzMaps(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data) override;
	bool GetKzMap(PluginId owner, const char *mapname, FKZ_ResponseCallback callback, void *data) override;
	bool GetKzMapRecords(PluginId owner, const char *mapname, FKZ_ResponseCallback callback, int limit, int offset, const char *sort,
						 void *data) override;
	bool GetKzMapCourses(PluginId owner, const char *mapname, FKZ_ResponseCallback callback, int limit, int offset, const char *sort,
						 void *data) override;

	// KZ Global - servers
	bool GetKzServers(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data) override;
	bool GetKzServer(PluginId owner, int id, FKZ_ResponseCallback callback, void *data) override;

	// KZ Global - bans
	bool GetKzBans(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data) override;
	bool GetKzActiveBans(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data) override;
	bool GetKzBan(PluginId owner, int id, FKZ_ResponseCallback callback, void *data) override;
	bool GetKzPlayerBans(PluginId owner, const char *steamid, FKZ_ResponseCallback callback, int limit, int offset, const char *sort,
						 void *data) override;

	// KZ Local (CS:GO)
	bool GetLocalMaps(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data) override;
	bool GetLocalMap(PluginId owner, const char *mapname, FKZ_ResponseCallback callback, void *data) override;
	bool GetLocalRecords(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data) override;
	bool GetLocalPlayers(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data) override;

	// KZ Local CS2
	bool GetLocalCS2Maps(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data) override;
	bool GetLocalCS2Records(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data) override;
	bool GetLocalCS2Players(PluginId owner, FKZ_ResponseCallback callback, int limit, int offset, const char *sort, void *data) override;
	bool GetLocalCS2Stats(PluginId owner, FKZ_ResponseCallback callback, void *data) override;
};

extern FKZApi g_FKZApi;

#endif // _INCLUDE_FKZ_API_H_
