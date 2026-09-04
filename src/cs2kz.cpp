/**
 * FKZ API - CS2KZ integration
 *
 * Reads the player's mode/timer state from cs2kz-metamod's public interface
 * and collects per-mode playtime for the status report.
 */

#include <cstdio>
#include <cstring>

#include "cs2kz.h"
#include "globals.h"
#include "json_builder.h"
#include "player_manager.h"
#include "plugin.h"
#include <ISmmAPI.h>
#include <ics2kz.h>
#include <tier0/platform.h>

ICS2KZ *g_pCS2KZ = NULL;

// Mode index -> cs2kz short name / API playtime_modes key.
// Only the two built-in modes are tracked.
enum
{
	MODE_VNL = 0,
	MODE_CKZ,
	MODE_COUNT
};

static const char *const s_modeShortNames[MODE_COUNT] = {"VNL", "CKZ"};
static const char *const s_modeApiKeys[MODE_COUNT] = {"cs2kz_vnl", "cs2kz_ckz"};

// How often the current mode is polled. cs2kz has no mode-change notification,
// so a switch is attributed to the wrong mode for at most this long.
static const double SAMPLE_INTERVAL = 1.0;

struct ModePlaytime
{
	double seconds[MODE_COUNT]; // delta since the last report
	double lastSample;          // Plat_FloatTime() of the last sample, 0 = not started
	int currentMode;            // mode the player was in at that sample, -1 = unknown
};

static ModePlaytime s_playtime[MAXPLAYERS + 1];
static double s_lastTick = 0.0;

void CS2KZ_Refresh()
{
	int ret = META_IFACE_FAILED;
	void *iface = g_SMAPI ? g_SMAPI->MetaFactory(CS2KZ_INTERFACE, &ret, NULL) : NULL;

	ICS2KZ *previous = g_pCS2KZ;
	g_pCS2KZ = (iface && ret == META_IFACE_OK) ? static_cast<ICS2KZ *>(iface) : NULL;

	if (g_pCS2KZ && !previous)
	{
		META_CONPRINTF("[FKZ] cs2kz-metamod detected (%s), mode data enabled\n", CS2KZ_INTERFACE);
	}
	else if (!g_pCS2KZ && previous)
	{
		META_CONPRINTF("[FKZ] cs2kz-metamod unloaded, mode data disabled\n");
	}
}

void CS2KZ_ResetPlayer(int slot)
{
	if (slot < 0 || slot >= MAXPLAYERS)
	{
		return;
	}
	memset(&s_playtime[slot], 0, sizeof(s_playtime[slot]));
	s_playtime[slot].currentMode = -1;
}

// The mode the player is in right now, or -1 when cs2kz has no mode for this slot.
static int GetCurrentMode(int slot)
{
	if (!g_pCS2KZ || !g_pCS2KZ->IsValidPlayer(slot))
	{
		return -1;
	}

	KZTimerStatus status;
	if (!g_pCS2KZ->GetTimerStatus(slot, &status) || !status.modeShortName[0])
	{
		return -1;
	}

	for (int m = 0; m < MODE_COUNT; m++)
	{
		if (strcmp(status.modeShortName, s_modeShortNames[m]) == 0)
		{
			return m;
		}
	}
	return -1;
}

// Flushes the time since the last sample into the mode the player was in back then, then latches the mode they are in now.
static void SamplePlayer(int slot, double now)
{
	ModePlaytime &pt = s_playtime[slot];

	if (pt.lastSample > 0.0 && now > pt.lastSample && pt.currentMode >= 0 && pt.currentMode < MODE_COUNT)
	{
		pt.seconds[pt.currentMode] += now - pt.lastSample;
	}

	pt.lastSample = now;
	pt.currentMode = GetCurrentMode(slot);
}

void CS2KZ_SampleAll()
{
	if (!g_pCS2KZ)
	{
		return;
	}

	double now = Plat_FloatTime();
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		const PlayerInfo &player = g_PlayerManager.GetPlayer(i);
		if (player.connected && player.inGame && !player.isBot)
		{
			SamplePlayer(i, now);
		}
	}
}

void CS2KZ_Tick()
{
	double now = Plat_FloatTime();
	if (now - s_lastTick < SAMPLE_INTERVAL)
	{
		return;
	}
	s_lastTick = now;

	CS2KZ_SampleAll();
}

void CS2KZ_ResetAllPlaytimeDeltas()
{
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		memset(s_playtime[i].seconds, 0, sizeof(s_playtime[i].seconds));
	}
}

std::string BuildCS2KZJson(int slot)
{
	if (!g_pCS2KZ || !g_pCS2KZ->IsValidPlayer(slot))
	{
		return "null";
	}

	KZTimerStatus status;
	if (!g_pCS2KZ->GetTimerStatus(slot, &status) || !status.modeName[0])
	{
		return "null";
	}

	char buf[32];
	std::string json = "{";
	json += "\"mode\":\"" + JsonEscape(status.modeName) + "\",";
	json += "\"mode_short\":\"" + JsonEscape(status.modeShortName) + "\",";
	json += std::string("\"timer_running\":") + (status.running ? "true" : "false") + ",";
	json += std::string("\"paused\":") + (status.paused ? "true" : "false") + ",";
	json += std::string("\"valid\":") + (status.valid ? "true" : "false") + ",";

	snprintf(buf, sizeof(buf), "%.3f", status.time);
	json += "\"time\":" + std::string(buf) + ",";

	json += "\"course\":" + (status.onCourse ? "\"" + JsonEscape(status.course.name) + "\"" : std::string("null")) + ",";
	json += "\"teleports\":" + std::to_string(status.teleportsUsed);
	json += "}";
	return json;
}

std::string BuildPlaytimeModesJson(int slot)
{
	std::string json = "{";

	for (int m = 0; m < MODE_COUNT; m++)
	{
		if (m > 0)
		{
			json += ",";
		}

		json += std::string("\"") + s_modeApiKeys[m] + "\":";

		double seconds = (slot >= 0 && slot < MAXPLAYERS) ? s_playtime[slot].seconds[m] : 0.0;
		if (seconds > 0.0)
		{
			char buf[32];
			snprintf(buf, sizeof(buf), "%.1f", seconds);
			json += buf;
		}
		else
		{
			// No time in this mode: keep the key visible, but let the API leave whatever it already accrued alone.
			json += "null";
		}
	}

	json += "}";
	return json;
}
