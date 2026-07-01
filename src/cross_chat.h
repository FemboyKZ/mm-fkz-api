#ifndef _INCLUDE_CROSS_CHAT_H_
#define _INCLUDE_CROSS_CHAT_H_

#include <cstdint>
#include <tier1/convar.h>

// Cross-server chat relay for CS2.
//
//   Send:    CrossChat_OnDispatchConCommand inspects every "say" the engine dispatches,
//            relays public lines to the API (/chat/messages),
//            and handles the !crosschat mute toggle. team chat is never relayed.
//   Receive: a single long-poll GET /chat/stream is kept open while humans are present;
//            ncoming messages are printed via SayText2.
//            CrossChat_Tick (re)opens the poll each game frame.

void CrossChat_OnDispatchConCommand(ConCommandRef cmd, const CCommandContext &ctx, const CCommand &args);

// Called every game frame.
// hasHumans gates the stream so empty servers hold noconnection.
void CrossChat_Tick(bool hasHumans);

// Load a player's saved cross-chat mute state from the local database (async).
// Call once the player has a valid steamID (ClientPutInServer).
void CrossChat_LoadPrefs(int slot, uint64_t steamId64);

// Clear a slot's mute state when the player leaves.
void CrossChat_OnClientDisconnect(int slot);

#endif // _INCLUDE_CROSS_CHAT_H_
