#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "config.h"
#include "http_client.h"
#include "plugin.h"

HttpClient g_HttpClient;

// One in-flight request.
// Owns its own CCallResult and frees itself once the handler has run.
class HttpRequestContext
{
public:
	HttpRequestContext(HttpResponseHandler handler, void *userData) : m_handler(handler), m_userData(userData), m_request(INVALID_HTTPREQUEST_HANDLE)
	{
	}

	void Start(HTTPRequestHandle req, SteamAPICall_t call)
	{
		m_request = req;
		m_callResult.Set(call, this, &HttpRequestContext::OnCompleted);
	}

	// Cancel a still-pending request (on unload).
	// Does not invoke the handler and does not touch the owner's pending list.
	void Cancel()
	{
		m_callResult.Cancel();
		ISteamHTTP *http = g_HttpClient.SteamHTTP();
		if (m_request != INVALID_HTTPREQUEST_HANDLE && http)
		{
			http->ReleaseHTTPRequest(m_request);
		}
		m_request = INVALID_HTTPREQUEST_HANDLE;
	}

private:
	void OnCompleted(HTTPRequestCompleted_t *pResult, bool bIOFailure)
	{
		ISteamHTTP *http = g_HttpClient.SteamHTTP();
		bool success = false;
		int statusCode = 0;
		std::string body;

		if (!bIOFailure && pResult->m_bRequestSuccessful)
		{
			statusCode = (int)pResult->m_eStatusCode;
			success = (statusCode >= 200 && statusCode < 300);

			if (http)
			{
				uint32 size = 0;
				if (http->GetHTTPResponseBodySize(pResult->m_hRequest, &size) && size > 0)
				{
					body.resize(size);
					http->GetHTTPResponseBodyData(pResult->m_hRequest, (uint8 *)&body[0], size);
					// Steam may count a trailing NUL in the size, trim for clean strings.
					while (!body.empty() && body.back() == '\0')
					{
						body.pop_back();
					}
				}
			}
		}

		if (http)
		{
			http->ReleaseHTTPRequest(pResult->m_hRequest);
		}
		m_request = INVALID_HTTPREQUEST_HANDLE;

		if (m_handler)
		{
			m_handler(success, statusCode, body.c_str(), (uint32)body.size(), m_userData);
		}

		// Deregister and self-destruct.
		// Steam's CCallResult marks itself invalid before invoking this handler, so deleting here is safe.
		g_HttpClient.OnContextFinished(this);
		delete this;
	}

	HttpResponseHandler m_handler;
	void *m_userData;
	HTTPRequestHandle m_request;
	CCallResult<HttpRequestContext, HTTPRequestCompleted_t> m_callResult;
};

static EHTTPMethod MethodFromString(const char *method)
{
	if (!stricmp(method, "POST"))
	{
		return k_EHTTPMethodPOST;
	}
	if (!stricmp(method, "PUT"))
	{
		return k_EHTTPMethodPUT;
	}
	if (!stricmp(method, "PATCH"))
	{
		return k_EHTTPMethodPATCH;
	}
	if (!stricmp(method, "DELETE"))
	{
		return k_EHTTPMethodDELETE;
	}
	if (!stricmp(method, "HEAD"))
	{
		return k_EHTTPMethodHEAD;
	}
	return k_EHTTPMethodGET;
}

HttpClient::HttpClient() : m_ready(false) {}

bool HttpClient::Init()
{
	if (m_ready)
	{
		return true;
	}

	m_ready = m_steamAPI.Init();
	if (m_ready && !m_steamAPI.SteamHTTP())
	{
		m_ready = false;
	}

	return m_ready;
}

void HttpClient::RunCallbacks()
{
	if (m_ready)
	{
		SteamGameServer_RunCallbacks();
	}
}

void HttpClient::OnContextFinished(HttpRequestContext *ctx)
{
	m_pending.erase(std::remove(m_pending.begin(), m_pending.end(), ctx), m_pending.end());
}

void HttpClient::ReleasePending()
{
	for (HttpRequestContext *ctx : m_pending)
	{
		ctx->Cancel();
		delete ctx;
	}
	m_pending.clear();
	m_ready = false;
}

bool HttpClient::Request(const char *method, const char *url, const char *body, uint32 timeoutSec, HttpResponseHandler handler, void *userData)
{
	ISteamHTTP *http = m_steamAPI.SteamHTTP();
	if (!http)
	{
		// Try reinitializing the Steam API.
		m_ready = m_steamAPI.Init();
		http = m_steamAPI.SteamHTTP();
		if (!http)
		{
			META_CONPRINTF("[FKZ] Steam HTTP not available, skipping %s %s\n", method, url);
			return false;
		}
		m_ready = true;
		META_CONPRINTF("[FKZ] Steam API recovered\n");
	}

	EHTTPMethod eMethod = MethodFromString(method);
	HTTPRequestHandle req = http->CreateHTTPRequest(eMethod, url);
	if (req == INVALID_HTTPREQUEST_HANDLE)
	{
		META_CONPRINTF("[FKZ] Failed to create HTTP request %s %s\n", method, url);
		return false;
	}

	http->SetHTTPRequestHeaderValue(req, "Accept", "application/json");

	if (g_Config.apiKey[0] != '\0')
	{
		char authHeader[300];
		snprintf(authHeader, sizeof(authHeader), "Bearer %s", g_Config.apiKey);
		http->SetHTTPRequestHeaderValue(req, "Authorization", authHeader);
	}

	http->SetHTTPRequestNetworkActivityTimeout(req, timeoutSec);

	if (body && body[0] != '\0' && (eMethod == k_EHTTPMethodPOST || eMethod == k_EHTTPMethodPUT || eMethod == k_EHTTPMethodPATCH))
	{
		http->SetHTTPRequestRawPostBody(req, "application/json", (uint8 *)body, (uint32)strlen(body));
	}

	SteamAPICall_t hCall;
	if (!http->SendHTTPRequest(req, &hCall))
	{
		META_CONPRINTF("[FKZ] Failed to send HTTP request %s %s\n", method, url);
		http->ReleaseHTTPRequest(req);
		return false;
	}

	HttpRequestContext *ctx = new HttpRequestContext(handler, userData);
	ctx->Start(req, hCall);
	m_pending.push_back(ctx);
	return true;
}
