#ifndef _INCLUDE_CS2KZ_H_
#define _INCLUDE_CS2KZ_H_

#include <string>

class ICS2KZ;

// CS2KZ integration (ICS2KZ001, exposed by cs2kz-metamod through MetaFactory).
//
// The interface is main-thread only, so everything here is called from GameFrame or from the report builder.
extern ICS2KZ *g_pCS2KZ;

// (Re)queries MetaFactory for the interface.
void CS2KZ_Refresh();

inline bool CS2KZ_IsLoaded()
{
	return g_pCS2KZ != NULL;
}

// Per-mode playtime tracking.
// cs2kz has no mode-change event, so the current mode is polled
// and the elapsed time is accrued into whichever mode the player was in at the previous sample.
void CS2KZ_ResetPlayer(int slot);
void CS2KZ_Tick();                   // throttled poll, call every frame
void CS2KZ_SampleAll();              // unthrottled flush, call before building a report
void CS2KZ_ResetAllPlaytimeDeltas(); // call after the report captured the deltas

// Per-player timer/mode object for the report, or "null" when cs2kz has nothing for this slot.
std::string BuildCS2KZJson(int slot);

// Per-player per-mode playtime deltas (seconds since the last report).
// Keys: cs2kz_vnl = Vanilla, cs2kz_ckz = Classic.
// A mode with no time accrued is sent as null so the key stays visible without clobbering what the API already accrued.
std::string BuildPlaytimeModesJson(int slot);

#endif // _INCLUDE_CS2KZ_H_
