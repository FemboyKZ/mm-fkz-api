#ifndef _INCLUDE_JSON_BUILDER_H_
#define _INCLUDE_JSON_BUILDER_H_

#include <string>

std::string JsonEscape(const char *str);
std::string BuildPayloadJson();
std::string BuildHibernateJson();

void ResolveIpPort(char *ip, int ipLen, int &port);

#endif // _INCLUDE_JSON_BUILDER_H_
