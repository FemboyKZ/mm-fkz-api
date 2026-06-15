/**
 * FKZ API - Status updater
 *
 * Periodically reports live player/server state to /servers/status and signals
 * hibernation when the server empties.
 */

#include <cstdio>
#include <string>

#include "api.h"
#include "config.h"
#include "json_builder.h"
#include "plugin.h"
#include "reporter.h"

static int g_failCount = 0;
static int g_successCount = 0;

static void OnReportResponse(bool /*success*/, int statusCode, const char *body,
                             void * /*data*/) {
  if (statusCode == 200) {
    if (g_failCount > 0)
      META_CONPRINTF("[FKZ] POST recovered after %d failures\n", g_failCount);
    g_failCount = 0;
    g_successCount++;
    if (g_successCount == 1 || g_successCount % 30 == 0)
      META_CONPRINTF("[FKZ] POST OK (count=%d)\n", g_successCount);
  } else if (statusCode == 0) {
    g_failCount++;
    META_CONPRINTF(
        "[FKZ] POST %s/servers/status transport error (no HTTP response: "
        "timeout/DNS/TLS/conn) (fail #%d)\n",
        g_Config.apiUrl, g_failCount);
  } else {
    g_failCount++;
    META_CONPRINTF("[FKZ] POST HTTP %d (fail #%d)\n", statusCode, g_failCount);
    if (statusCode >= 301 && statusCode <= 308)
      META_CONPRINTF("[FKZ] Redirect detected - update api_url in config to "
                     "the final URL\n");
  }
}

static void OnHibernateResponse(bool /*success*/, int statusCode,
                                const char * /*body*/, void * /*data*/) {
  if (statusCode != 200)
    META_CONPRINTF("[FKZ] Hibernate signal returned HTTP %d\n", statusCode);
}

void SendReport() {
  std::string payload = BuildPayloadJson();
  g_FKZApi.PostServerStatus(payload.c_str(), OnReportResponse, NULL);
}

void SendHibernate() {
  if (g_Config.apiUrl[0] == '\0')
    return;

  std::string payload = BuildHibernateJson();
  if (g_FKZApi.PostHibernate(payload.c_str(), OnHibernateResponse, NULL))
    META_CONPRINTF("[FKZ] Sent hibernate signal (server empty)\n");
}
