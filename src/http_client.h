#ifndef _INCLUDE_HTTP_CLIENT_H_
#define _INCLUDE_HTTP_CLIENT_H_

#include <steam/isteamhttp.h>
#include <steam/steam_gameserver.h>
#include <vector>

// Invoked when a request completes (or fails in transit).
//   success    : HTTP status was 2xx
//   statusCode : HTTP status, or 0 on transport/IO failure
//   body       : null-terminated response body (never null, empty when none)
//   bodyLen    : length of body
//   userData   : opaque value passed to Request()
typedef void (*HttpResponseHandler)(bool success, int statusCode,
                                    const char *body, uint32 bodyLen,
                                    void *userData);

class HttpRequestContext; // defined in http_client.cpp

class HttpClient {
public:
  HttpClient();

  bool Init();
  bool IsReady() const { return m_ready; }

  // Dispatches an async request against an absolute URL.
  // method is GET/POST/PUT/PATCH/DELETE/HEAD (case-insensitive).
  // body (JSON string) is sent for POST/PUT/PATCH, pass nullptr otherwise.
  // Returns true if the request was dispatched (the handler fires later) and
  // false if it could not be sent (the handler is NOT called in that case).
  bool Request(const char *method, const char *url, const char *body,
               uint32 timeoutSec, HttpResponseHandler handler, void *userData);

  // Pump Steam game server callbacks so completion handlers fire.
  void RunCallbacks();

  // Cancel and free all in-flight requests (call on unload).
  void ReleasePending();

  ISteamHTTP *SteamHTTP() { return m_steamAPI.SteamHTTP(); }

  // Called by a context when it completes normally, to deregister itself.
  void OnContextFinished(HttpRequestContext *ctx);

private:
  CSteamGameServerAPIContext m_steamAPI;
  bool m_ready;
  std::vector<HttpRequestContext *> m_pending;
};

extern HttpClient g_HttpClient;

#endif // _INCLUDE_HTTP_CLIENT_H_
