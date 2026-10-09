/**
 * FKZ API - Status updater
 *
 * Periodically reports live player/server state to /servers/status and signals hibernation when the server empties.
 */

#include <cstdint>
#include <cstdio>
#include <string>

#include "api.h"
#include "config.h"
#include "cs2kz.h"
#include "json_builder.h"
#include "plugin.h"
#include "reporter.h"

#include "utils/log.h"

static int g_failCount = 0;
static int g_successCount = 0;

static void OnReportResponse(bool /*success*/, int statusCode, const char *body, void *data)
{
	CS2KZ_OnReportResult((int)(intptr_t)data, statusCode == 200);

	if (statusCode == 200)
	{
		if (g_failCount > 0)
		{
			MMU_LOG_INFO("POST recovered after %d failures\n", g_failCount);
		}
		g_failCount = 0;
		g_successCount++;
		if (g_successCount == 1 || g_successCount % 30 == 0)
		{
			MMU_LOG_INFO("POST OK (count=%d)\n", g_successCount);
		}
	}
	else if (statusCode == 0)
	{
		g_failCount++;
		MMU_LOG_WARN("POST %s/servers/status transport error (no HTTP response: "
					 "timeout/DNS/TLS/conn) (fail #%d)\n",
					 g_Config.apiUrl.c_str(), g_failCount);
	}
	else
	{
		g_failCount++;
		MMU_LOG_WARN("POST HTTP %d (fail #%d)\n", statusCode, g_failCount);
		if (statusCode >= 301 && statusCode <= 308)
		{
			MMU_LOG_WARN("Redirect detected - update api_url in config to "
						 "the final URL\n");
		}
	}
}

static void OnHibernateResponse(bool /*success*/, int statusCode, const char * /*body*/, void * /*data*/)
{
	if (statusCode != 200)
	{
		MMU_LOG_WARN("Hibernate signal returned HTTP %d\n", statusCode);
	}
}

void SendReport()
{
	// Flush the time elapsed since the last sample into each player's active mode before snapshotting.
	CS2KZ_SampleAll();

	std::string payload = BuildPayloadJson();

	// The per-mode deltas are in the payload now, held aside until the POST is answered so a failure can give them back.
	int reportId = CS2KZ_TakePlaytimeDeltas();

	if (!g_FKZApi.PostServerStatus(g_PLID, payload.c_str(), OnReportResponse, (void *)(intptr_t)reportId))
	{
		CS2KZ_OnReportResult(reportId, false);
	}
}

void SendHibernate()
{
	if (g_Config.apiUrl.empty())
	{
		return;
	}

	std::string payload = BuildHibernateJson();
	if (g_FKZApi.PostHibernate(g_PLID, payload.c_str(), OnHibernateResponse, NULL))
	{
		MMU_LOG_INFO("Sent hibernate signal (server empty)\n");
	}
}
