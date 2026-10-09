#ifndef _INCLUDE_SERVER_INFO_H_
#define _INCLUDE_SERVER_INFO_H_

struct ServerInfo
{
	char hostname[256];
	char mmVersion[64];
	char osName[16];
	char mapName[256];
	char version[64];
	int tickrate;
	bool secure;

	ServerInfo();
	void Cache();
	// What changes after startup: server.cfg sets the hostname, Steam answers later.
	void Refresh();
	void UpdateMap();
};

extern ServerInfo g_ServerInfo;

#endif // _INCLUDE_SERVER_INFO_H_
