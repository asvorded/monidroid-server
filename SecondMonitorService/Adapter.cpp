#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>

#include "Adapter.h"

#include "../MonidroidInfo/Monidroid.h"

void DebugPrint(const wchar_t* format, ...) {
    wchar_t buffer[1024];
    va_list args;
    va_start(args, format);
    vswprintf(buffer, 1024, format, args);
    va_end(args);
    OutputDebugStringW(buffer);
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

HRESULT CreateVirtualAdapter() {
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
    HRESULT hr = SwDeviceCreate(L"MonidroidDriver", L"HTREE\\ROOT\\0",
        &createInfo, 0, nullptr, CreationCallback, &hEvent, &hSwDevice);
    if (FAILED(hr)) {
        return hr;
    }

    // Wait for callback to signal that the device has been created
    DebugPrint(L"Waiting for device to be created....\n");
    DWORD waitResult = WaitForSingleObject(hEvent, 5 * 1000);
    if (waitResult != WAIT_OBJECT_0) {
        DebugPrint(L"Wait for device creation failed\n");
        return E_FAIL;
    }
    DebugPrint(L"Device created\n\n");
    return S_OK;
}

DWORD MonidroidInitGraphicsAdapter(HANDLE* pHandle) {
    *pHandle = INVALID_HANDLE_VALUE;

    // Try open device if it already created
    HANDLE hAdapter = CreateFileW(MONIDROID_USER_DEVICE_PATH,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    );
    if (hAdapter != INVALID_HANDLE_VALUE) {
        *pHandle = hAdapter;
        return 0;
    }

    // Else try to create it
    HRESULT hr = CreateVirtualAdapter();
    if (FAILED(hr)) {
        DebugPrint(L"SwDeviceCreate failed with 0x%lx\n", hr);
        return hr;
    }

    SetLastError(0);

    // Now try open the device
    int retriesCount = 5;
    for (int i = 1; i <= retriesCount; i++) {
        DebugPrint(L"CreateFile attempt %d...\n", i);
        hAdapter = CreateFileW(MONIDROID_USER_DEVICE_PATH,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
        );
        if (hAdapter != INVALID_HANDLE_VALUE) {
            break;
        } else {
            
            Sleep(1000);
        }
    }
    DWORD err = GetLastError();
    DebugPrint(L"Second CreateFile ended with code %d\n", err);
    *pHandle = hAdapter;
    return err;
}

DWORD MonidroidDestroyGraphicsAdapter() {
    return 0;
}
