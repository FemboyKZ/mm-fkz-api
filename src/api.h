#ifndef _INCLUDE_FKZ_API_H_
#define _INCLUDE_FKZ_API_H_

#include "../include/IFKZApi.h"

// Concrete implementation of the public FKZ API interface.
// Exposed to other Metamod plugins via MMSPlugin::OnMetamodQuery().
class FKZApi : public IFKZApi {
public:
  int GetInterfaceVersion() override { return 1; }

  // Core
  bool ApiRequest(const char *method, const char *path, const char *body,
                  FKZ_ResponseCallback callback, void *data) override;
  int GetApiBase(char *buffer, int maxlength) override;
  bool GetHealth(FKZ_ResponseCallback callback, void *data) override;
  bool PostServerStatus(const char *body, FKZ_ResponseCallback callback,
                        void *data) override;
  bool PostHibernate(const char *body, FKZ_ResponseCallback callback,
                     void *data) override;

  // Live servers / players / maps
  bool GetServers(FKZ_ResponseCallback callback, int limit, int offset,
                  const char *sort, void *data) override;
  bool GetServer(const char *ip, FKZ_ResponseCallback callback,
                 void *data) override;
  bool GetPlayers(FKZ_ResponseCallback callback, int limit, int offset,
                  const char *sort, void *data) override;
  bool GetOnlinePlayers(FKZ_ResponseCallback callback, int limit, int offset,
                        const char *sort, void *data) override;
  bool GetPlayer(const char *steamid, FKZ_ResponseCallback callback,
                 void *data) override;
  bool GetMaps(FKZ_ResponseCallback callback, int limit, int offset,
               const char *sort, void *data) override;
  bool GetMap(const char *mapname, FKZ_ResponseCallback callback,
              void *data) override;

  // KZ Global - records
  bool GetKzRecords(FKZ_ResponseCallback callback, int limit, int offset,
                    const char *sort, void *data) override;
  bool GetKzRecentRecords(FKZ_ResponseCallback callback, int limit, int offset,
                          const char *sort, void *data) override;
  bool GetKzWorldRecords(FKZ_ResponseCallback callback, int limit, int offset,
                         const char *sort, void *data) override;
  bool GetKzLeaderboard(const char *mapname, FKZ_ResponseCallback callback,
                        int limit, int offset, const char *sort,
                        void *data) override;
  bool GetKzRecord(int id, FKZ_ResponseCallback callback, void *data) override;

  // KZ Global - players
  bool GetKzPlayers(FKZ_ResponseCallback callback, int limit, int offset,
                    const char *sort, void *data) override;
  bool GetKzPlayer(const char *steamid, FKZ_ResponseCallback callback,
                   void *data) override;
  bool GetKzPlayerRecords(const char *steamid, FKZ_ResponseCallback callback,
                          int limit, int offset, const char *sort,
                          void *data) override;
  bool GetKzPlayerPBs(const char *steamid, FKZ_ResponseCallback callback,
                      int limit, int offset, const char *sort,
                      void *data) override;
  bool GetKzPlayerCompletions(const char *steamid,
                              FKZ_ResponseCallback callback, int limit,
                              int offset, const char *sort,
                              void *data) override;

  // KZ Global - maps
  bool GetKzMaps(FKZ_ResponseCallback callback, int limit, int offset,
                 const char *sort, void *data) override;
  bool GetKzMap(const char *mapname, FKZ_ResponseCallback callback,
                void *data) override;
  bool GetKzMapRecords(const char *mapname, FKZ_ResponseCallback callback,
                       int limit, int offset, const char *sort,
                       void *data) override;
  bool GetKzMapCourses(const char *mapname, FKZ_ResponseCallback callback,
                       int limit, int offset, const char *sort,
                       void *data) override;

  // KZ Global - servers
  bool GetKzServers(FKZ_ResponseCallback callback, int limit, int offset,
                    const char *sort, void *data) override;
  bool GetKzServer(int id, FKZ_ResponseCallback callback, void *data) override;

  // KZ Global - bans
  bool GetKzBans(FKZ_ResponseCallback callback, int limit, int offset,
                 const char *sort, void *data) override;
  bool GetKzActiveBans(FKZ_ResponseCallback callback, int limit, int offset,
                       const char *sort, void *data) override;
  bool GetKzBan(int id, FKZ_ResponseCallback callback, void *data) override;
  bool GetKzPlayerBans(const char *steamid, FKZ_ResponseCallback callback,
                       int limit, int offset, const char *sort,
                       void *data) override;

  // KZ Local (CS:GO)
  bool GetLocalMaps(FKZ_ResponseCallback callback, int limit, int offset,
                    const char *sort, void *data) override;
  bool GetLocalMap(const char *mapname, FKZ_ResponseCallback callback,
                   void *data) override;
  bool GetLocalRecords(FKZ_ResponseCallback callback, int limit, int offset,
                       const char *sort, void *data) override;
  bool GetLocalPlayers(FKZ_ResponseCallback callback, int limit, int offset,
                       const char *sort, void *data) override;

  // KZ Local CS2
  bool GetLocalCS2Maps(FKZ_ResponseCallback callback, int limit, int offset,
                       const char *sort, void *data) override;
  bool GetLocalCS2Records(FKZ_ResponseCallback callback, int limit, int offset,
                          const char *sort, void *data) override;
  bool GetLocalCS2Players(FKZ_ResponseCallback callback, int limit, int offset,
                          const char *sort, void *data) override;
  bool GetLocalCS2Stats(FKZ_ResponseCallback callback, void *data) override;
};

extern FKZApi g_FKZApi;

#endif // _INCLUDE_FKZ_API_H_
