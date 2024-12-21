#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <strsafe.h>
#include <dxgi1_5.h>
#include <d3d11_1.h>
#include <wincodec.h>
#include <iostream>
#include <list>

#include "Adapter.h"
#include "Installer.h"

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