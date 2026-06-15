#ifndef _INCLUDE_CS2KZ_H_
#define _INCLUDE_CS2KZ_H_

#include <string>

// CS2KZ per-gamemode playtime for the status report.
//
// JSON keys: cs2kz_vnl = Vanilla, cs2kz_ckz = Classic.
std::string BuildPlaytimeModesJson(int slot);

#endif // _INCLUDE_CS2KZ_H_
