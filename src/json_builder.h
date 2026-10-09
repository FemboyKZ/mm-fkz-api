#ifndef _INCLUDE_JSON_BUILDER_H_
#define _INCLUDE_JSON_BUILDER_H_

#include <string>

std::string JsonEscape(const char *str);

// In game, human and confirmed by Steam. Until then the SteamID a report would carry is only the client's claim.
bool IsReportedPlayer(int slot);
std::string BuildPayloadJson();
std::string BuildHibernateJson();

void ResolveIpPort(char *ip, int ipLen, int &port);

#endif // _INCLUDE_JSON_BUILDER_H_
