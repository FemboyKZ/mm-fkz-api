#ifndef _INCLUDE_FKZ_JSON_BUILDER_H_
#define _INCLUDE_FKZ_JSON_BUILDER_H_

#include <string>

std::string FKZ_JsonEscape(const char *str);

// In game, human and confirmed by Steam. Until then the SteamID a report would carry is only the client's claim.
bool FKZ_IsReportedPlayer(int slot);
std::string FKZ_BuildPayloadJson();
std::string FKZ_BuildHibernateJson();

void FKZ_ResolveIpPort(char *ip, int ipLen, int &port);

#endif // _INCLUDE_FKZ_JSON_BUILDER_H_
