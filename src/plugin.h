/**
 * FKZ API Plugin (Metamod:Source, CS2 / x86_64)
 *
 * Configuration: <game>/cfg/fkz-api/core.cfg
 */

#ifndef _INCLUDE_METAMOD_SOURCE_PLUGIN_H_
#define _INCLUDE_METAMOD_SOURCE_PLUGIN_H_

#include "version_gen.h"
#include <ISmmPlugin.h>
#include <iserver.h>
#include <tier1/convar.h>

class MMSPlugin : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late);
	bool Unload(char *error, size_t maxlen);
	void AllPluginsLoaded();

	// Exposes the IFKZApi interface to other Metamod plugins (MetaFactory).
	void *OnMetamodQuery(const char *iface, int *ret) override;

	// Re-query cs2kz-metamod's interface
	void OnPluginLoad(PluginId id) override;
	void OnPluginUnload(PluginId id) override;

	MMSPlugin();

	// KHook callbacks
	KHook::Return<void> Hook_GameFrame(ISource2Server *, bool simulating, bool bFirstTick, bool bLastTick);
	KHook::Return<void> Hook_ClientPutInServer(ISource2GameClients *, CPlayerSlot slot, char const *pszName, int type, uint64 xuid);
	KHook::Return<void> Hook_ClientDisconnect(ISource2GameClients *, CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName,
											  uint64 xuid, const char *pszNetworkID);
	KHook::Return<void> Hook_OnClientConnected(ISource2GameClients *, CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID,
											   const char *pszAddress, bool bFakePlayer);
	KHook::Return<void> Hook_StartupServer(INetworkServerService *, const GameSessionConfiguration_t &config, ISource2WorldSession *, const char *);
	KHook::Return<void> Hook_ServerHibernationUpdate(ISource2Server *, bool bHibernating);
	KHook::Return<void> Hook_DispatchConCommand(ICvar *, ConCommandRef cmd, const CCommandContext &ctx, const CCommand &args);

public:
	const char *GetAuthor()
	{
		return PLUGIN_AUTHOR;
	}

	const char *GetName()
	{
		return PLUGIN_DISPLAY_NAME;
	}

	const char *GetDescription()
	{
		return PLUGIN_DESCRIPTION;
	}

	const char *GetURL()
	{
		return PLUGIN_URL;
	}

	const char *GetLicense()
	{
		return PLUGIN_LICENSE;
	}

	const char *GetVersion()
	{
		return PLUGIN_FULL_VERSION;
	}

	const char *GetDate()
	{
		return __DATE__;
	}

	const char *GetLogTag()
	{
		return PLUGIN_LOGTAG;
	}

private:
	double m_lastReportTime;
	bool m_serverActive;

	KHook::Virtual<ISource2Server, void, bool, bool, bool> m_GameFrame;
	KHook::Virtual<ISource2Server, void, bool> m_ServerHibernationUpdate;
	KHook::Virtual<ISource2GameClients, void, CPlayerSlot, char const *, int, uint64> m_ClientPutInServer;
	KHook::Virtual<ISource2GameClients, void, CPlayerSlot, ENetworkDisconnectionReason, const char *, uint64, const char *> m_ClientDisconnect;
	KHook::Virtual<ISource2GameClients, void, CPlayerSlot, const char *, uint64, const char *, const char *, bool> m_OnClientConnected;
	KHook::Virtual<INetworkServerService, void, const GameSessionConfiguration_t &, ISource2WorldSession *, const char *> m_StartupServer;
	KHook::Virtual<ICvar, void, ConCommandRef, const CCommandContext &, const CCommand &> m_DispatchConCommand;
};

extern MMSPlugin g_ThisPlugin;

PLUGIN_GLOBALVARS();

#endif //_INCLUDE_METAMOD_SOURCE_PLUGIN_H_
