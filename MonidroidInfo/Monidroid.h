#pragma once

#include <devioctl.h>
#include <initguid.h>

#include <WinSock2.h>

#define MONIDROID_SERVICE_VERSION				"0.0.1"
#define MAKE_MONIDROID_VERSION_MESSAGE(pretext, aftertext)	pretext  MONIDROID_SERVICE_VERSION  aftertext

#define MONIDROID_DEVICE_PATH					L"\\Device\\MonidroidAdapter"
#define MONIDROID_USER_DEVICE_PATH				L"\\\\.\\GLOBALROOT\\Device\\MonidroidAdapter"

#define IOCTL_IDDCX_MONITOR_CONNECT				CTL_CODE(FILE_DEVICE_VIDEO, 0xA10, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IDDCX_MONITOR_DISCONNECT			CTL_CODE(FILE_DEVICE_VIDEO, 0xA11, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IDDCX_REQUEST_FRAME				CTL_CODE(FILE_DEVICE_VIDEO, 0xA12, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IDDCX_INIT_FRAME_SEND				CTL_CODE(FILE_DEVICE_VIDEO, 0xA21, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IDDCX_FINALIZE_FRAME_SEND			CTL_CODE(FILE_DEVICE_VIDEO, 0xA22, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IDDCX_SEND_FRAME					CTL_CODE(FILE_DEVICE_VIDEO, 0xA23, METHOD_BUFFERED, FILE_ANY_ACCESS)

// {ADF1470D-14D2-46AD-9499-896FDD87E215}
DEFINE_GUID(MonidroidGroupGuid, 0xadf1470d, 0x14d2, 0x46ad, 0x94, 0x99, 0x89, 0x6f, 0xdd, 0x87, 0xe2, 0x15);

struct ADAPTER_MONITOR_INFO {
	UINT connectorIndex; // OUT
	LUID adapterLuid;    // OUT
	DWORD driverProcessId; // OUT

	SOCKET monitorNumberBySocket; // IN

	int width;  // IN
	int height; // IN
	int hertz;  // IN
};

struct FRAME_MONITOR_INFO {
	UINT connectorIndex;   // IN
	HANDLE hDriverHandle;  // OUT
};

struct INIT_SEND_INFO {
	UINT connectorIndex;				// IN
	WSAPROTOCOL_INFOW clientSocketInfo; // IN
};

// Service

#define MY_SERVICE_NAME				L"Monidroid service"
#define MY_APP_NAME					L"Monidroid"

#define MONIDROID_PORT_SZ "14765"
#define MONIDROID_PORT 14765

// Protocol

#define WELCOME_WORD				"WELCOME"
#define WELCOME_WORD_LEN			7

#define FRAME_WORD					"FRAME"
#define FRAME_WORD_LEN				5

#define CLIENT_ECHO_WORD			"MDCLIENT_ECHO"

#define SERVER_ECHO_WORD			"MDIDD_ECHO"
