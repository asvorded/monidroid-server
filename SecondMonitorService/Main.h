#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <WinSock2.h>

struct ClientInfo {

	ClientInfo(SOCKET socket) : 
		clientSocket(socket),
		adapterLuid(),
		driverProcessId(0),
		connectorIndex(~0),
		width(-1),
		height(-1),
		hertz(-1),
		hCommunicationThread(nullptr)
	{}

	void SetupClient(int width, int height, int hertz) {
		this->width = width;
		this->height = height;
		this->hertz = hertz;
	}

	SOCKET clientSocket;

	UINT connectorIndex;
	LUID adapterLuid;
	DWORD driverProcessId;

	int width;
	int height;
	int hertz;

	HANDLE hCommunicationThread;
};

int InitService();
void FinalizeService();

void AppMain();
