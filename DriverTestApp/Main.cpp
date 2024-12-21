#include <iostream>
#include <vector>

#include <windows.h>
#include <swdevice.h>
#include <conio.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <strsafe.h>

#include "../MonidroidInfo/Monidroid.h"

void TryConnectMonitor() {
    DWORD err;
    HANDLE hAdapter = CreateFileW(MONIDROID_USER_DEVICE_PATH,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    );

    if (hAdapter == INVALID_HANDLE_VALUE) {
        err = GetLastError();
        printf("Device open failed, code %d (%x)\n", err, err);
        return;
    }
    printf("Device open successful\n");
    
    LPVOID inBuf = calloc(5, 1);
    LPVOID outBuf = calloc(5, 1);
    DWORD bytesReturned;

    if (DeviceIoControl(hAdapter, IOCTL_IDDCX_MONITOR_CONNECT,
        inBuf, 5, outBuf, 5, &bytesReturned, NULL)
    ) {
        printf("DeviceIoControl succedded");
    } else {
        err = GetLastError();
        printf("DeviceIoControl failed, error code %d (0x%08x)", err, err);
    }
    
    CloseHandle(hAdapter);
    
}

VOID WINAPI
CreationCallback(
    _In_ HSWDEVICE hSwDevice,
    _In_ HRESULT hrCreateResult,
    _In_opt_ PVOID pContext,
    _In_ PCWSTR pszDeviceInstanceId
) {
    HANDLE hEvent = *(HANDLE*)pContext;
    SetEvent(hEvent);

    UNREFERENCED_PARAMETER(hSwDevice);
    UNREFERENCED_PARAMETER(hrCreateResult);
}

int __cdecl main(int argc, wchar_t* argv[]) {
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    HSWDEVICE hSwDevice;
    SW_DEVICE_CREATE_INFO createInfo = { 0 };
    PCWSTR description = L"Monidroid IddCx SWD adapter";

    // These match the Pnp id's in the inf file so OS will load the driver when the device is created
    PCWSTR instanceId = L"0001";
    PCWSTR hardwareIds = L"MonidroidDriver\0\0";
    PCWSTR compatibleIds = L"MonidroidDriver\0\0";

    createInfo.cbSize = sizeof(createInfo);
    createInfo.pszzCompatibleIds = compatibleIds;
    createInfo.pszInstanceId = instanceId;
    createInfo.pszzHardwareIds = hardwareIds;
    createInfo.pszDeviceDescription = description;

    createInfo.CapabilityFlags = SWDeviceCapabilitiesRemovable |
        SWDeviceCapabilitiesSilentInstall |
        SWDeviceCapabilitiesDriverRequired;

    // Create the device
    HRESULT hr = SwDeviceCreate(L"MonidroidDriver",
        L"HTREE\\ROOT\\0",
        &createInfo,
        0,
        nullptr,
        CreationCallback,
        &hEvent,
        &hSwDevice);
    if (FAILED(hr)) {
        printf("SwDeviceCreate failed with 0x%lx\n", hr);
        return 1;
    }

    // Wait for callback to signal that the device has been created
    printf("Waiting for device to be created....\n");
    DWORD waitResult = WaitForSingleObject(hEvent, 10 * 1000);
    if (waitResult != WAIT_OBJECT_0) {
        printf("Wait for device creation failed\n");
        return 1;
    }
    printf("Device created\n\n");

    // Now wait for user to indicate the device should be stopped
    printf("Press 'x' to exit and destory the software device\n");
    bool bExit = false;
    do {
        // Wait for key press
        int key = _getch();

        if (key == 'o') {
            TryConnectMonitor();
        }

        if (key == 'x' || key == 'X') {
            bExit = true;
        }
    } while (!bExit);

    // Stop the device, this will cause the sample to be unloaded
    SwDeviceClose(hSwDevice);

    return 0;
}