#include <cstring>
#include <cstdio>

#include "http_client.h"
#include "config.h"
#include "plugin.h"

HttpClient g_HttpClient;

HttpClient::HttpClient()
	: m_pendingRequest(INVALID_HTTPREQUEST_HANDLE)
	, m_ready(false)
	, m_failCount(0)
	, m_successCount(0)
{
}

bool HttpClient::Init()
{
	if (m_ready) return true;

	m_ready = m_steamAPI.Init();
	if (m_ready && !m_steamAPI.SteamHTTP())
		m_ready = false;

	return m_ready;
}

void HttpClient::ReleasePending()
{
	m_httpCallResult.Cancel();

	if (m_pendingRequest != INVALID_HTTPREQUEST_HANDLE && m_steamAPI.SteamHTTP())
	{
		m_steamAPI.SteamHTTP()->ReleaseHTTPRequest(m_pendingRequest);
		m_pendingRequest = INVALID_HTTPREQUEST_HANDLE;
	}

	m_ready = false;
}

void HttpClient::Post(const char *url, const std::string &body, uint32 timeoutSec)
{
	ISteamHTTP *pHTTP = m_steamAPI.SteamHTTP();
	if (!pHTTP)
	{
		// Try reinitializing Steam API
		m_ready = m_steamAPI.Init();
		pHTTP = m_steamAPI.SteamHTTP();
		if (!pHTTP)
		{
			META_CONPRINTF("[FKZ] Steam HTTP not available, skipping request to %s\n", url);
			return;
		}
		m_ready = true;
		META_CONPRINTF("[FKZ] Steam API recovered\n");
	}

	// Cancel any pending request
	if (m_pendingRequest != INVALID_HTTPREQUEST_HANDLE)
	{
		pHTTP->ReleaseHTTPRequest(m_pendingRequest);
		m_pendingRequest = INVALID_HTTPREQUEST_HANDLE;
	}

	HTTPRequestHandle req = pHTTP->CreateHTTPRequest(k_EHTTPMethodPOST, url);
	if (req == INVALID_HTTPREQUEST_HANDLE)
	{
		META_CONPRINTF("[FKZ] Failed to create HTTP request to %s\n", url);
		return;
	}

	pHTTP->SetHTTPRequestHeaderValue(req, "Content-Type", "application/json");

	if (g_Config.apiKey[0] != '\0')
	{
		char authHeader[300];
		snprintf(authHeader, sizeof(authHeader), "Bearer %s", g_Config.apiKey);
		pHTTP->SetHTTPRequestHeaderValue(req, "Authorization", authHeader);
	}

	pHTTP->SetHTTPRequestNetworkActivityTimeout(req, timeoutSec);
	pHTTP->SetHTTPRequestRawPostBody(req, "application/json",
		(uint8 *)body.c_str(), static_cast<uint32>(body.size()));

	SteamAPICall_t hCall;
	if (!pHTTP->SendHTTPRequest(req, &hCall))
	{
		META_CONPRINTF("[FKZ] Failed to send HTTP request to %s\n", url);
		pHTTP->ReleaseHTTPRequest(req);
		return;
	}

	m_pendingRequest = req;
	m_httpCallResult.Set(hCall, this, &HttpClient::OnHTTPRequestCompleted);
}

void HttpClient::OnHTTPRequestCompleted(HTTPRequestCompleted_t *pResult, bool bIOFailure)
{
	if (bIOFailure || !pResult->m_bRequestSuccessful)
	{
		m_failCount++;
		META_CONPRINTF("[FKZ] HTTP request failed (IO failure, fail #%d)\n", m_failCount);
	}
	else if (pResult->m_eStatusCode == k_EHTTPStatusCode200OK)
	{
		if (m_failCount > 0)
			META_CONPRINTF("[FKZ] POST recovered after %d failures\n", m_failCount);
		m_failCount = 0;
		m_successCount++;
		if (m_successCount == 1 || m_successCount % 30 == 0)
			META_CONPRINTF("[FKZ] POST OK (count=%d)\n", m_successCount);
	}
	else
	{
		m_failCount++;
		META_CONPRINTF("[FKZ] HTTP %d (fail #%d)\n", pResult->m_eStatusCode, m_failCount);

		if (pResult->m_eStatusCode >= 301 && pResult->m_eStatusCode <= 308)
			META_CONPRINTF("[FKZ] Redirect detected. Update api_url in config to the final URL\n");
	}

	// Release the request handle
	if (m_steamAPI.SteamHTTP())
		m_steamAPI.SteamHTTP()->ReleaseHTTPRequest(pResult->m_hRequest);

	if (m_pendingRequest == pResult->m_hRequest)
		m_pendingRequest = INVALID_HTTPREQUEST_HANDLE;
}
