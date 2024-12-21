#include "Driver.h"


static const IndirectMonitorInfo::IndirectMonitorMode SAMPLE_MONITOR_MODES[] = {
    { 1920, 1080, 60 },
    { 1024, 768, 60 },
    { 640, 480, 60 }
};

static const IndirectMonitorInfo SAMPLE_MONITOR_INFO = {
    // Modified EDID from Dell S2719DGF
    {
        0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x10,0xAC,0xE6,0xD0,0x55,0x5A,0x4A,0x30,0x24,0x1D,0x01,
        0x04,0xA5,0x3C,0x22,0x78,0xFB,0x6C,0xE5,0xA5,0x55,0x50,0xA0,0x23,0x0B,0x50,0x54,0x00,0x02,0x00,
        0xD1,0xC0,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x58,0xE3,0x00,
        0xA0,0xA0,0xA0,0x29,0x50,0x30,0x20,0x35,0x00,0x55,0x50,0x21,0x00,0x00,0x1A,0x00,0x00,0x00,0xFF,
        0x00,0x37,0x4A,0x51,0x58,0x42,0x59,0x32,0x0A,0x20,0x20,0x20,0x20,0x20,0x00,0x00,0x00,0xFC,0x00,
        0x53,0x32,0x37,0x31,0x39,0x44,0x47,0x46,0x0A,0x20,0x20,0x20,0x20,0x00,0x00,0x00,0xFD,0x00,0x28,
        0x9B,0xFA,0xFA,0x40,0x01,0x0A,0x20,0x20,0x20,0x20,0x20,0x20,0x00,0x2C
    },
    {
        { 2560, 1440,  60 },
        { 1920, 1080,  60 },
        { 1024,  768,  60 },
    },
    0
};

/*
* ----- Helper functions
*/
template <typename T> void CoSafeRelease(T** ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = nullptr;
    }
}

void CppSafeRelease(void** ppObj) {
    if (*ppObj) {
        delete* ppObj;
        *ppObj = nullptr;
    }
}

static inline void FillSignalInfo(DISPLAYCONFIG_VIDEO_SIGNAL_INFO& Mode, DWORD Width, DWORD Height, DWORD VSync, bool bMonitorMode) {
    Mode.totalSize.cx = Mode.activeSize.cx = Width;
    Mode.totalSize.cy = Mode.activeSize.cy = Height;

    Mode.AdditionalSignalInfo.vSyncFreqDivider = bMonitorMode ? 0 : 1;
    Mode.AdditionalSignalInfo.videoStandard = 255;

    Mode.vSyncFreq.Numerator = VSync;
    Mode.vSyncFreq.Denominator = 1;
    Mode.hSyncFreq.Numerator = VSync * Height;
    Mode.hSyncFreq.Denominator = 1;

    Mode.scanLineOrdering = DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE;

    Mode.pixelRate = ((UINT64)VSync) * ((UINT64)Width) * ((UINT64)Height);
}

static IDDCX_MONITOR_MODE CreateIddCxMonitorMode(DWORD Width, DWORD Height, DWORD VSync, IDDCX_MONITOR_MODE_ORIGIN Origin = IDDCX_MONITOR_MODE_ORIGIN_DRIVER) {
    IDDCX_MONITOR_MODE Mode = {};

    Mode.Size = sizeof(Mode);
    Mode.Origin = Origin;
    FillSignalInfo(Mode.MonitorVideoSignalInfo, Width, Height, VSync, true);

    return Mode;
}

static IDDCX_TARGET_MODE CreateIddCxTargetMode(DWORD Width, DWORD Height, DWORD VSync) {
    IDDCX_TARGET_MODE Mode = {};

    Mode.Size = sizeof(Mode);
    FillSignalInfo(Mode.TargetVideoSignalInfo.targetVideoSignalInfo, Width, Height, VSync, false);

    return Mode;
}

extern "C" DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD EvtIddDriverDeviceAdd;

EVT_WDF_DEVICE_FILE_CREATE EvtIddCxFileCreate;
EVT_WDF_FILE_CLOSE EvtIddCxFileClose;
EVT_IDD_CX_DEVICE_IO_CONTROL EvtIddCxDeviceIoControl;

EVT_IDD_CX_PARSE_MONITOR_DESCRIPTION EvtIddCxParseMonitorDescription;
EVT_IDD_CX_ADAPTER_COMMIT_MODES EvtIddCxAdapterCommitModes;

EVT_IDD_CX_MONITOR_GET_DEFAULT_DESCRIPTION_MODES EvtIddCxMonitorGetDefaultDescriptionModes;
EVT_IDD_CX_MONITOR_QUERY_TARGET_MODES EvtIddCxMonitorQueryTargetModes;
EVT_IDD_CX_MONITOR_ASSIGN_SWAPCHAIN EvtIddCxMonitorAssignSwapChain;
EVT_IDD_CX_MONITOR_UNASSIGN_SWAPCHAIN EvtIddCxMonitorUnassignSwapChain;

EVT_IDD_CX_ADAPTER_INIT_FINISHED EvtIddCxAdapterInitFinished;

EVT_WDF_DEVICE_D0_ENTRY EvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT EvtDeviceD0Exit;

// Declare context types
struct AdapterContextWrapper {
    AdapterContext* pContext;

    void Cleanup() {
        delete pContext;
        pContext = nullptr;
    }
};

struct MonitorContextWrapper {
    MonitorContext* pContext;

    void Cleanup() {
        delete pContext;
        pContext = nullptr;
    }
};

WDF_DECLARE_CONTEXT_TYPE(AdapterContextWrapper);

WDF_DECLARE_CONTEXT_TYPE(MonitorContextWrapper);

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, UINT dwReason, LPVOID lpReserved) {
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(lpReserved);
    UNREFERENCED_PARAMETER(dwReason);

    return TRUE;
}

_Use_decl_annotations_
extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT driverObject, IN PUNICODE_STRING registryPath) {
    NTSTATUS status;

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, EvtIddDriverDeviceAdd);

    WDFDRIVER driver;

    status = WdfDriverCreate(driverObject, registryPath, &attributes, &config, &driver);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS EvtIddDriverDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit) {

    UNREFERENCED_PARAMETER(Driver);

    // Initialize PnP callbacks
    WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);

    pnpCallbacks.EvtDeviceD0Entry = EvtDeviceD0Entry;
    pnpCallbacks.EvtDeviceD0Exit = EvtDeviceD0Exit;

    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpCallbacks);

    // Initialize IddCx
    IDD_CX_CLIENT_CONFIG config;
    IDD_CX_CLIENT_CONFIG_INIT(&config);

    // Register I/O control
    config.EvtIddCxDeviceIoControl = EvtIddCxDeviceIoControl;

    config.EvtIddCxParseMonitorDescription = EvtIddCxParseMonitorDescription;
    config.EvtIddCxAdapterCommitModes = EvtIddCxAdapterCommitModes;

    config.EvtIddCxMonitorGetDefaultDescriptionModes = EvtIddCxMonitorGetDefaultDescriptionModes;
    config.EvtIddCxMonitorQueryTargetModes = EvtIddCxMonitorQueryTargetModes;
    config.EvtIddCxMonitorAssignSwapChain = EvtIddCxMonitorAssignSwapChain;
    config.EvtIddCxMonitorUnassignSwapChain = EvtIddCxMonitorUnassignSwapChain;

    config.EvtIddCxAdapterInitFinished = EvtIddCxAdapterInitFinished;

    NTSTATUS status = IddCxDeviceInitConfig(DeviceInit, &config);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Register file handling
    WDF_FILEOBJECT_CONFIG fileConfig;
    WDF_FILEOBJECT_CONFIG_INIT(&fileConfig, EvtIddCxFileCreate, EvtIddCxFileClose, WDF_NO_EVENT_CALLBACK);

    WDF_OBJECT_ATTRIBUTES fileAttrs;
    WDF_OBJECT_ATTRIBUTES_INIT(&fileAttrs);
    fileAttrs.SynchronizationScope = WdfSynchronizationScopeNone;

    WdfDeviceInitSetFileObjectConfig(DeviceInit, &fileConfig, &fileAttrs);

    // Create and initialize device
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, AdapterContextWrapper);
    attributes.EvtCleanupCallback = [](WDFOBJECT object) {
        AdapterContextWrapper* pContext = WdfObjectGet_AdapterContextWrapper(object);
        if (pContext) {
            pContext->Cleanup();
        }
    };

    WDFDEVICE device;
    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Register device symbolic link
    DECLARE_CONST_UNICODE_STRING(symbolicLink, MONIDROID_DEVICE_PATH);

    status = WdfDeviceCreateSymbolicLink(device, &symbolicLink);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    AdapterContextWrapper* pContext = WdfObjectGet_AdapterContextWrapper(device);
    pContext->pContext = new AdapterContext(device);
    
    status = IddCxDeviceInitialize(device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    return STATUS_SUCCESS;
}

/*
* ---- PnP and power management
*/
#pragma region PnP and power management
// Device enabled (enter D0 state)
NTSTATUS EvtDeviceD0Entry(WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState) {
    UNREFERENCED_PARAMETER(PreviousState);
    WSADATA wsaData;

    int code = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (code != 0) {
        return STATUS_INTERNAL_ERROR;
    }

    auto* adapterContext = WdfObjectGet_AdapterContextWrapper(Device);
    adapterContext->pContext->Init();

    return STATUS_SUCCESS;
}

#pragma endregion

// Device disabled (exit D0 state)
NTSTATUS EvtDeviceD0Exit(WDFDEVICE Device, WDF_POWER_DEVICE_STATE TargetState) {
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(TargetState);

    WSACleanup();

    return STATUS_SUCCESS;
}

/*
* ---- IddCx Callbacks
*/
#pragma region IddCx Callbacks

void EvtIddCxFileCreate(WDFDEVICE Device, WDFREQUEST Request, WDFFILEOBJECT FileObject) {
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(FileObject);

    WdfRequestComplete(Request, STATUS_SUCCESS);
}

void EvtIddCxFileClose(WDFFILEOBJECT FileObject) {
    UNREFERENCED_PARAMETER(FileObject);
}

/// <summary>
/// Device I/O control function, used to control adapter from service
/// </summary>
void EvtIddCxDeviceIoControl(WDFDEVICE Device, WDFREQUEST Request,
    size_t OutputBufferLength, size_t InputBufferLength,
    ULONG IoControlCode
) {
    NTSTATUS status = 0;
    auto* pContext = WdfObjectGet_AdapterContextWrapper(Device);

    WDFMEMORY inMemory = nullptr;
    WDFMEMORY outMemory = nullptr;

    WdfRequestRetrieveInputMemory(Request, &inMemory);
    WdfRequestRetrieveOutputMemory(Request, &outMemory);

    ULONG_PTR information = 0;
    PVOID inBuf = WdfMemoryGetBuffer(inMemory, NULL);

    switch (IoControlCode) {
    case IOCTL_IDDCX_MONITOR_CONNECT:
    case IOCTL_IDDCX_MONITOR_DISCONNECT:
    {
        ADAPTER_MONITOR_INFO* pMonitorInfo = (ADAPTER_MONITOR_INFO*)inBuf;

        if (IoControlCode == IOCTL_IDDCX_MONITOR_CONNECT) {
            status = pContext->pContext->ConnectMonitor(pMonitorInfo);
        } else {
            status = pContext->pContext->DisconnectMonitor(pMonitorInfo);
        }

        WdfMemoryCopyFromBuffer(outMemory, 0, inBuf, sizeof(ADAPTER_MONITOR_INFO));
        information = sizeof(ADAPTER_MONITOR_INFO);
        break;
    }
    case IOCTL_IDDCX_REQUEST_FRAME:
    {
        status = STATUS_INVALID_PARAMETER;
        //FRAME_MONITOR_INFO* pFrameInfo = (FRAME_MONITOR_INFO*)inBuf;
        //status = pContext->pContext->FrameRequest(pFrameInfo);
        //WdfMemoryCopyFromBuffer(outMemory, 0, inBuf, sizeof(FRAME_MONITOR_INFO));
        //information = sizeof(FRAME_MONITOR_INFO);
        break;
    }
    case IOCTL_IDDCX_INIT_FRAME_SEND:
    {
        INIT_SEND_INFO* pSendInfo = (INIT_SEND_INFO*)inBuf;

        DWORD error = pContext->pContext->InitFrameSending(pSendInfo);
        status = STATUS_SUCCESS;
        
        WdfMemoryCopyFromBuffer(outMemory, 0, &error, sizeof(error));
        information = sizeof(error);
        break;
    }
    case IOCTL_IDDCX_SEND_FRAME:
    {
        UINT connectorIndex = *((UINT*)inBuf);

        HRESULT hr = pContext->pContext->SendNextFrame(connectorIndex);
        status = STATUS_SUCCESS;

        WdfMemoryCopyFromBuffer(outMemory, 0, &hr, sizeof(hr));
        information = sizeof(hr);
        break;
    }
    case IOCTL_IDDCX_FINALIZE_FRAME_SEND:
    {
        UINT connectorIndex = *((UINT*)inBuf);

        pContext->pContext->FinalizeFrameSending(connectorIndex);
        status = STATUS_SUCCESS;

        information = 0;
        break;
    }
    default:
        status = STATUS_INVALID_PARAMETER;
        break;
    }

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    WdfRequestCompleteWithInformation(Request, status, information);
}

/// <summary>
/// Called by system when adapter init has finished
/// </summary>
NTSTATUS EvtIddCxAdapterInitFinished(
    IDDCX_ADAPTER AdapterObject,
    const IDARG_IN_ADAPTER_INIT_FINISHED* pInArgs
) {
    UNREFERENCED_PARAMETER(AdapterObject);

    if (!NT_SUCCESS(pInArgs->AdapterInitStatus)) {
        return pInArgs->AdapterInitStatus;
    }

    return STATUS_SUCCESS;
}

/// <summary>
/// Called by OS to ask the driver to parse a EDID monitor description
/// into a list of modes that the monitor supports.
/// This function is called only if monitor had provided an EDID block
/// </summary>
/// <param name="pInArgs"></param>
/// <param name="pOutArgs"></param>
/// <returns>NTSTATUS value</returns>
NTSTATUS EvtIddCxParseMonitorDescription(
    const IDARG_IN_PARSEMONITORDESCRIPTION* pInArgs,
    IDARG_OUT_PARSEMONITORDESCRIPTION* pOutArgs
) {
    pOutArgs->MonitorModeBufferOutputCount = IndirectMonitorInfo::szModeList;

    if (pInArgs->MonitorModeBufferInputCount < IndirectMonitorInfo::szModeList) {
        return (pInArgs->MonitorModeBufferInputCount > 0) ? STATUS_BUFFER_TOO_SMALL : STATUS_SUCCESS;
    } else {

    }

    return STATUS_SUCCESS;
}

/// <summary>
/// Called by OS to get monitor modes when EDID block is not provided
/// </summary>
NTSTATUS EvtIddCxMonitorGetDefaultDescriptionModes(
    IDDCX_MONITOR MonitorObject,
    const IDARG_IN_GETDEFAULTDESCRIPTIONMODES* pInArgs,
    IDARG_OUT_GETDEFAULTDESCRIPTIONMODES* pOutArgs
) {
    UNREFERENCED_PARAMETER(MonitorObject);

    if (pInArgs->DefaultMonitorModeBufferInputCount == 0) {
        pOutArgs->DefaultMonitorModeBufferOutputCount = ARRAYSIZE(SAMPLE_MONITOR_MODES);
    } else {
        for (DWORD i = 0; i < ARRAYSIZE(SAMPLE_MONITOR_MODES); i++) {
            pInArgs->pDefaultMonitorModes[i] = CreateIddCxMonitorMode(
                SAMPLE_MONITOR_MODES[i].Width,
                SAMPLE_MONITOR_MODES[i].Height,
                SAMPLE_MONITOR_MODES[i].VSync,
                IDDCX_MONITOR_MODE_ORIGIN_DRIVER
            );
        }

        pOutArgs->DefaultMonitorModeBufferOutputCount = ARRAYSIZE(SAMPLE_MONITOR_MODES);
        pOutArgs->PreferredMonitorModeIdx = 0;
    }

    return STATUS_SUCCESS;
}

NTSTATUS EvtIddCxMonitorQueryTargetModes(
    IDDCX_MONITOR MonitorObject,
    const IDARG_IN_QUERYTARGETMODES* pInArgs,
    IDARG_OUT_QUERYTARGETMODES* pOutArgs
) {
    UNREFERENCED_PARAMETER(MonitorObject);

    // Create a set of modes supported for frame processing and scan-out. These are typically not based on the
    // monitor's descriptor and instead are based on the static processing capability of the device. The OS will
    // report the available set of modes for a given output as the intersection of monitor modes with target modes.

    pOutArgs->TargetModeBufferOutputCount = 3; // TODO : hardcoded value

    if (pInArgs->TargetModeBufferInputCount >= 3) {
        for (int i = 0; i < 3; i++) {
            pInArgs->pTargetModes[i] = CreateIddCxTargetMode(
                SAMPLE_MONITOR_MODES[i].Width, SAMPLE_MONITOR_MODES[i].Height, SAMPLE_MONITOR_MODES[i].VSync
            );
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS EvtIddCxAdapterCommitModes(
    IDDCX_ADAPTER AdapterObject,
    const IDARG_IN_COMMITMODES* pInArgs
) {
    UNREFERENCED_PARAMETER(AdapterObject);
    UNREFERENCED_PARAMETER(pInArgs);

    return STATUS_SUCCESS;
}

NTSTATUS EvtIddCxMonitorAssignSwapChain(
    IDDCX_MONITOR MonitorObject,
    const IDARG_IN_SETSWAPCHAIN* pInArgs
) {
    auto* pContext = WdfObjectGet_MonitorContextWrapper(MonitorObject);
    HRESULT hr = pContext->pContext->AssignSwapChain(
        pInArgs->hSwapChain, pInArgs->RenderAdapterLuid, pInArgs->hNextSurfaceAvailable
    );
    if (FAILED(hr)) {
        return STATUS_GRAPHICS_INDIRECT_DISPLAY_ABANDON_SWAPCHAIN;
    }

    return STATUS_SUCCESS;
}

NTSTATUS EvtIddCxMonitorUnassignSwapChain(IDDCX_MONITOR MonitorObject) {
    auto* pContext = WdfObjectGet_MonitorContextWrapper(MonitorObject);
    pContext->pContext->UnassignSwapChain();

    return STATUS_SUCCESS;
}

#pragma endregion

/*
* ---- Adapter functionality
*/
#pragma region Adapter functionality
/// <summary>
/// Set WDF device field
/// </summary>
/// <param name="device">WDF device created by WdfDeviceCreate</param>
AdapterContext::AdapterContext(WDFDEVICE Device) : device(Device) {
    adapter = {};
    for (int i = 0; i < MAX_MONITOR_COUNT; i++) {
        connectedMonitors[i].monitorObject = nullptr;
        connectedMonitors[i].monitorNumberBySocket = INVALID_SOCKET;
    }
}

AdapterContext::~AdapterContext() { }

/// <summary>
/// Initializes display adapter
/// </summary>
/// <returns>NTSTATUS value</returns>
NTSTATUS AdapterContext::Init() {

    IDDCX_ADAPTER_CAPS adapterCaps = {};
    adapterCaps.Size = sizeof(IDDCX_ADAPTER_CAPS);

    // Basic feature support
    adapterCaps.MaxMonitorsSupported = 5;
    adapterCaps.EndPointDiagnostics.Size = sizeof(adapterCaps.EndPointDiagnostics);
    adapterCaps.EndPointDiagnostics.TransmissionType = IDDCX_TRANSMISSION_TYPE_WIRED_OTHER;
    adapterCaps.EndPointDiagnostics.GammaSupport = IDDCX_FEATURE_IMPLEMENTATION_NONE;

    // Device strings for telemetry
    adapterCaps.EndPointDiagnostics.pEndPointFriendlyName = L"Monidroid Graphics Adapter";
    adapterCaps.EndPointDiagnostics.pEndPointManufacturerName = L"Maxim Dadush";
    adapterCaps.EndPointDiagnostics.pEndPointModelName = L"Monidroid Virtual Monitor";

    // Hardware and firmware versions
    IDDCX_ENDPOINT_VERSION version = {};
    version.Size = sizeof(version);
    version.MajorVer = 1;

    adapterCaps.EndPointDiagnostics.pFirmwareVersion = &version;
    adapterCaps.EndPointDiagnostics.pHardwareVersion = &version;

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, AdapterContextWrapper);

    IDARG_IN_ADAPTER_INIT adapterInit = {};
    IDARG_OUT_ADAPTER_INIT adapterInitOut = {};

    adapterInit.ObjectAttributes = &attributes;
    adapterInit.pCaps = &adapterCaps;
    adapterInit.WdfDevice = device;

    NTSTATUS status = IddCxAdapterInitAsync(&adapterInit, &adapterInitOut);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    this->adapter = adapterInitOut.AdapterObject;

    auto* pContext = WdfObjectGet_AdapterContextWrapper(adapterInitOut.AdapterObject);
    pContext->pContext = this;

    return STATUS_SUCCESS;
}

/// <summary>
/// Called when new monitor was connected
/// </summary>
NTSTATUS AdapterContext::ConnectMonitor(ADAPTER_MONITOR_INFO* pMonitorInfo, bool edidProvided) {
    // Find the closest slot
    int connectorIndex = 0;
    while (connectedMonitors[connectorIndex].monitorObject != NULL && connectorIndex < MAX_MONITOR_COUNT) connectorIndex++;
    if (connectorIndex == MAX_MONITOR_COUNT) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Fill monitor info
    WDF_OBJECT_ATTRIBUTES attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr, MonitorContextWrapper);
    attr.EvtCleanupCallback = [](WDFOBJECT object) {
        auto* pContext = WdfObjectGet_MonitorContextWrapper(object);
        if (pContext) {
            pContext->Cleanup();
        }
    };

    IDDCX_MONITOR_INFO monitorInfo = {};
    monitorInfo.Size = sizeof(IDDCX_MONITOR_INFO);
    monitorInfo.ConnectorIndex = connectorIndex;
    monitorInfo.MonitorContainerId = MonidroidGroupGuid;
    monitorInfo.MonitorType = DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HDMI;

    monitorInfo.MonitorDescription.Size = sizeof(monitorInfo.MonitorDescription);

    if (edidProvided) {
        monitorInfo.MonitorDescription.Type = IDDCX_MONITOR_DESCRIPTION_TYPE_EDID;
        monitorInfo.MonitorDescription.pData = (PVOID)SAMPLE_MONITOR_INFO.pEdidBlock;
        monitorInfo.MonitorDescription.DataSize = SAMPLE_MONITOR_INFO.szEdidBlock;
    } else {
        // EDID is not provided by default
        monitorInfo.MonitorDescription.Type = IDDCX_MONITOR_DESCRIPTION_TYPE_EDID;
        monitorInfo.MonitorDescription.pData = NULL;
        monitorInfo.MonitorDescription.DataSize = 0;
    }

    IDARG_IN_MONITORCREATE monitorCreate = {};
    monitorCreate.ObjectAttributes = &attr;
    monitorCreate.pMonitorInfo = &monitorInfo;

    IDARG_OUT_MONITORCREATE monitorCreateOut;

    NTSTATUS status = IddCxMonitorCreate(adapter, &monitorCreate, &monitorCreateOut);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Set up monitor context
    auto* pContext = WdfObjectGet_MonitorContextWrapper(monitorCreateOut.MonitorObject);
    pContext->pContext = new MonitorContext(monitorCreateOut.MonitorObject);

    // Add monitor to monitor list
    connectedMonitors[connectorIndex].monitorObject = monitorCreateOut.MonitorObject;
    connectedMonitors[connectorIndex].monitorNumberBySocket = pMonitorInfo->monitorNumberBySocket;
    pMonitorInfo->connectorIndex = connectorIndex;
    pContext->pContext->SetupMonitor(pMonitorInfo);

    // Tell the OS that the monitor has been plugged in
    IDARG_OUT_MONITORARRIVAL monitorArrivalOut;
    status = IddCxMonitorArrival(monitorCreateOut.MonitorObject, &monitorArrivalOut);

    // Pass data to enable frames processing
    pMonitorInfo->adapterLuid = monitorArrivalOut.OsAdapterLuid;
    pMonitorInfo->driverProcessId = GetCurrentProcessId();

    return status;
}

NTSTATUS AdapterContext::FrameRequest(FRAME_MONITOR_INFO* pFrameInfo) {

    // get handle to frame
    IDDCX_MONITOR monitorObject = connectedMonitors[pFrameInfo->connectorIndex].monitorObject;
    auto* pContext = WdfObjectGet_MonitorContextWrapper(monitorObject);
    HANDLE hDriverFrame;
    HRESULT hr = pContext->pContext->GetFrameFromChain(&hDriverFrame);
    if (FAILED(hr)) {
        return STATUS_INVALID_PARAMETER;
    }

    // Pass texture handle to caller
    pFrameInfo->hDriverHandle = hDriverFrame;

    return STATUS_SUCCESS;
}

/// <summary>
/// Initialization of frames sending by driver
/// </summary>
DWORD AdapterContext::InitFrameSending(INIT_SEND_INFO* pSendInfo) {
    IDDCX_MONITOR monitorObject = connectedMonitors[pSendInfo->connectorIndex].monitorObject;
    auto* pContext = WdfObjectGet_MonitorContextWrapper(monitorObject);

    return pContext->pContext->InitFrameSending(pSendInfo);
}

/// <summary>
/// Sending next frame by driver
/// </summary>
HRESULT AdapterContext::SendNextFrame(UINT connectorIndex) {
    IDDCX_MONITOR monitorObject = connectedMonitors[connectorIndex].monitorObject;
    auto* pContext = WdfObjectGet_MonitorContextWrapper(monitorObject);

    return pContext->pContext->SendNextFrame();
}

/// <summary>
/// Finalization of frames sending by driver
/// </summary>
void AdapterContext::FinalizeFrameSending(UINT connectorIndex) {
    IDDCX_MONITOR monitorObject = connectedMonitors[connectorIndex].monitorObject;
    auto* pContext = WdfObjectGet_MonitorContextWrapper(monitorObject);
    pContext->pContext->FinalizeFrameSending();
}


NTSTATUS AdapterContext::DisconnectMonitor(ADAPTER_MONITOR_INFO* pMonitorInfo) {
    // Find slot
    int connectorIndex = 0;
    while (connectedMonitors[connectorIndex].monitorNumberBySocket != pMonitorInfo->monitorNumberBySocket
        && connectorIndex < MAX_MONITOR_COUNT) connectorIndex++;
    if (connectorIndex == MAX_MONITOR_COUNT) {
        return STATUS_NOT_FOUND;
    }

    IDDCX_MONITOR m = connectedMonitors[connectorIndex].monitorObject;
    connectedMonitors[connectorIndex].monitorObject = nullptr;
    connectedMonitors[connectorIndex].monitorNumberBySocket = INVALID_SOCKET;

    return IddCxMonitorDeparture(m);
}

#pragma endregion

/*
* ------ Monitor functionality
*/
#pragma region Monitor functionality

MonitorContext::MonitorContext(IDDCX_MONITOR Monitor) :
    swapChain(nullptr),
    pDevice(nullptr),
    pDeviceContext(nullptr),
    hProcessingThread(nullptr),
    hNextFrameAvailable(nullptr),
    hStopEvent(nullptr)
{
    adapterLuid = {};
    info.monitorObject = Monitor;

    // init frames array
    for (int i = 0; i < MAX_FRAMES_COUNT; i++) {
        framesChain[i] = nullptr;
    }
    currentFrame = -1;

    // init sockets
    clientSocket = INVALID_SOCKET;

    InitializeCriticalSection(&syncRoot);
}

MonitorContext::~MonitorContext() {
    DeleteCriticalSection(&syncRoot);
}

void MonitorContext::SetupMonitor(ADAPTER_MONITOR_INFO* pMonitirInfo) {
    info.width = pMonitirInfo->width;
    info.height = pMonitirInfo->height;
    info.hertz = pMonitirInfo->hertz;
}

/// <summary>
/// Assigns swapchain when monitor is plugged
/// </summary>
HRESULT MonitorContext::AssignSwapChain(IDDCX_SWAPCHAIN SwapChain, LUID AdapterLuid, HANDLE hNextSurfaceAvailable) {
    this->swapChain = SwapChain;
    this->adapterLuid = AdapterLuid;
    this->hNextFrameAvailable = hNextSurfaceAvailable;

    HRESULT hr = InitProcessor();
    if (SUCCEEDED(hr)) {
        StartProcessor();
        return S_OK;
    } else {
        // Always delete the swap-chain object in order to kick the system
        // to provide a new swap-chain if necessary.
        WdfObjectDelete(SwapChain);
        //CppSafeRelease((void**)&pProcessor);
        FinalizeProcessor();
        return hr;
    }
}

NTSTATUS MonitorContext::UnassignSwapChain() {
    StopProcessor();
    FinalizeProcessor();
    //CppSafeRelease((void**)&pProcessor);
    return STATUS_SUCCESS;
}

void MonitorContext::FinalizeProcessor() {
    swapChain = nullptr;
    adapterLuid = {};
    hNextFrameAvailable = NULL;

    CoSafeRelease(&pDevice);
    CoSafeRelease(&pDeviceContext);

    hProcessingThread = NULL;
    hStopEvent = NULL;

    for (int i = 0; i < MAX_FRAMES_COUNT; i++) {
        CoSafeRelease(&framesChain[i]);
    }
    currentFrame = -1;
}

//DXGIProcessor::DXGIProcessor(IDDCX_SWAPCHAIN SwapChain, LUID AdapterLuid, HANDLE hNextSurfaceAvailable) :
//    swapChain(SwapChain),
//    adapterLuid(AdapterLuid),
//    hProcessingThread(NULL),
//    hNextFrameAvailable(hNextSurfaceAvailable),
//    hStopEvent(NULL),
//    pDevice(nullptr),
//    pDeviceContext(nullptr)
//{ }
//
//DXGIProcessor::~DXGIProcessor() {
//}

HRESULT MonitorContext::InitProcessor() {
    IDXGIFactory5* pFactory = nullptr;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&pFactory));
    if (FAILED(hr)) {
        goto end;
    }

    IDXGIAdapter* pAdapter = nullptr;
    hr = pFactory->EnumAdapterByLuid(adapterLuid, IID_PPV_ARGS(&pAdapter));
    if (FAILED(hr)) {
        goto end;
    }

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1,
    };

    ID3D11Device* pDevice0 = nullptr;
    ID3D11DeviceContext* pDeviceContext0 = nullptr;
    hr = D3D11CreateDevice(pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        featureLevels, 7, D3D11_SDK_VERSION, &pDevice0, NULL, &pDeviceContext0
    );
    if (FAILED(hr)) {
        goto end;
    }

    hr = pDevice0->QueryInterface(IID_PPV_ARGS(&pDevice));
    if (FAILED(hr)) {
        goto end;
    }

    hr = pDeviceContext0->QueryInterface(IID_PPV_ARGS(&pDeviceContext));
    if (FAILED(hr)) {
        goto end;
    }

    // Init frames array
    for (int i = 0; i < MAX_FRAMES_COUNT; i++) {
        framesChain[i] = nullptr;
    }
    currentFrame = -1;

end:
    CoSafeRelease(&pFactory);
    CoSafeRelease(&pAdapter);
    CoSafeRelease(&pDevice0);
    CoSafeRelease(&pDeviceContext0);
    return hr;
}

DWORD MonitorContext::StartProcessor() {
    hStopEvent = CreateEventW(NULL, FALSE, FALSE, NULL);

    hProcessingThread = CreateThread(NULL, 0, MyThreadProc, this, 0, NULL);
    if (hProcessingThread == NULL) {
        return GetLastError();
    }

    return 0;
}

DWORD MonitorContext::StopProcessor() {
    SetEvent(hStopEvent);
    if (hProcessingThread) {
        WaitForSingleObject(hProcessingThread, INFINITE);
        hProcessingThread = NULL;
    }
    return 0;
}

DWORD WINAPI MonitorContext::MyThreadProc(LPVOID pContext) {
    return HRESULT_CODE(
        ((MonitorContext*)pContext)->ProcessorFunc()
    );
}

HRESULT MonitorContext::ProcessorFunc() {
    DWORD taskIndex = 0;
    HANDLE avTaskHandle = AvSetMmThreadCharacteristicsW(L"Distribution", &taskIndex);

    HRESULT hr = ProcessorMain();

    // Always delete the swap-chain object when swap-chain processing loop terminates
    // in order to kick the system to provide a new swap-chain if necessary.
    WdfObjectDelete(swapChain);
    swapChain = nullptr;

    AvRevertMmThreadCharacteristics(avTaskHandle);
    return hr;
}

HRESULT MonitorContext::ProcessorMain() {
    IDARG_IN_SWAPCHAINSETDEVICE setDevice;
    IDXGIDevice* pDxgiDevice;
    HRESULT hr = pDevice->QueryInterface(IID_PPV_ARGS(&pDxgiDevice));
    if (FAILED(hr)) {
        return hr;
    }

    setDevice.pDevice = pDxgiDevice;
    hr = IddCxSwapChainSetDevice(swapChain, &setDevice);
    if (FAILED(hr)) {
        return hr;
    }

    // Main processing loop
    while (true) {
        IDARG_OUT_RELEASEANDACQUIREBUFFER bufferArgs;
        hr = IddCxSwapChainReleaseAndAcquireBuffer(swapChain, &bufferArgs);

        if (hr == E_PENDING) {
            HANDLE waitHandles[] = {
                hNextFrameAvailable,
                hStopEvent
            };

            DWORD waitCode = WaitForMultipleObjects(ARRAYSIZE(waitHandles), waitHandles, FALSE, 1000 / 60);
            if (waitCode == WAIT_TIMEOUT || waitCode == WAIT_OBJECT_0) {
                // We have a new buffer
                continue;
            } else if (waitCode == WAIT_OBJECT_0 + 1) {
                // We must stop processing
                break;
            }
        } else if (SUCCEEDED(hr)) {
            IDXGIResource1* pSurface = nullptr;
            ID3D11Texture2D* pTexture = nullptr;
            D3D11_TEXTURE2D_DESC desc = {};

            // Get display surface
            bufferArgs.MetaData.pSurface->QueryInterface(IID_PPV_ARGS(&pSurface));
            pSurface->QueryInterface(IID_PPV_ARGS(&pTexture));
            
            pTexture->GetDesc(&desc);
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc.BindFlags = 0;//D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

            // Create display surface copy to pass it to service
            ID3D11Texture2D* pSharedTexture = nullptr;
            hr = pDevice->CreateTexture2D(&desc, NULL, &pSharedTexture);
            if (SUCCEEDED(hr)) {
                pDeviceContext->CopyResource(pSharedTexture, _Notnull_ pTexture);

                IDXGIResource1* pSharedSurface = nullptr;
                hr = pSharedTexture->QueryInterface(IID_PPV_ARGS(&pSharedSurface));
                PutFrameToChain(pSharedSurface);
            }
            
            // We have finished processing this frame hence we release the reference on it.
            CoSafeRelease(&pSurface);
            CoSafeRelease(&pTexture);
            CoSafeRelease(&bufferArgs.MetaData.pSurface);

            hr = IddCxSwapChainFinishedProcessingFrame(swapChain);
            if (FAILED(hr)) {
                return hr;
            }
        } else {
            return hr;
        }
    }
    return S_OK;
}

void MonitorContext::PutFrameToChain(IDXGIResource1* pFrame) {
    EnterCriticalSection(&syncRoot);

    currentFrame = (currentFrame + 1) / MAX_FRAMES_COUNT; // ++ and & 0x1111
    if (framesChain[currentFrame]) {
        //IDXGIResource1* pIntermediateFrame = framesChain[currentFrame]; // here references count equals 2
        //framesChain[currentFrame] = pFrame;                             // but here again 1
        CoSafeRelease(&framesChain[currentFrame]);
    }
    framesChain[currentFrame] = pFrame;

    LeaveCriticalSection(&syncRoot);
}

HRESULT MonitorContext::GetFrameFromChain(HANDLE* phFrame) {
    HRESULT hr;
    EnterCriticalSection(&syncRoot);
    if (currentFrame < 0 || framesChain[currentFrame] == nullptr) {
        hr = E_PENDING;
    } else {
        IDXGIResource1* pFrame = framesChain[currentFrame];
        hr = pFrame->GetSharedHandle(phFrame);
    }
    LeaveCriticalSection(&syncRoot);
    return hr;
}

/*
*  ----- Frames sending by the driver
*/

/// <summary>
/// Initializes frames sending by the driver
/// </summary>
DWORD MonitorContext::InitFrameSending(INIT_SEND_INFO* pSendInfo) {
    clientSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, &pSendInfo->clientSocketInfo, NULL, 0);
    if (clientSocket == INVALID_SOCKET) {
        return WSAGetLastError();
    }

    return 0;
}

HRESULT MonitorContext::SendNextFrame() {
    HRESULT hr = S_OK;

    // Try get next frame
    EnterCriticalSection(&syncRoot);
    if (currentFrame < 0 || framesChain[currentFrame] == nullptr) {
        LeaveCriticalSection(&syncRoot);

        // send 0 bytes to client
        int nullSize = 0;
        int bytesSent = send(clientSocket, (char*)&nullSize, sizeof(nullSize), 0);
        if (bytesSent == 0 || bytesSent == SOCKET_ERROR) {
            DWORD err = WSAGetLastError();
            UNREFERENCED_PARAMETER(err);
            return E_FAIL;
        }
        return S_OK;
    }

    IDXGIResource1* pResource = framesChain[currentFrame];
    framesChain[currentFrame] = nullptr;

    LeaveCriticalSection(&syncRoot);

    // Process frame
    ID3D11Texture2D* pTexture = nullptr;
    D3D11_TEXTURE2D_DESC desc = {};
    D3D11_MAPPED_SUBRESOURCE mappedResource = {};

    IWICBitmap* pWicBitmap = nullptr;
    IWICImagingFactory* pWicFactory = nullptr;
    IWICBitmapEncoder* pEncoder = nullptr;
    IWICBitmapFrameEncode* pFrame = nullptr;
    IStream* pOutStream = nullptr;

    pResource->QueryInterface(IID_PPV_ARGS(&pTexture));
    pTexture->GetDesc(&desc);

    hr = pDeviceContext->Map(pTexture, 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr)) {
        goto done;
    }

    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pWicFactory));
    if (FAILED(hr)) {
        goto done;
    }

    // --- Create picture
    hr = pWicFactory->CreateBitmapFromMemory(desc.Width, desc.Height,
        GUID_WICPixelFormat32bppRGBA, mappedResource.RowPitch,
        desc.Height * mappedResource.RowPitch, (BYTE*)mappedResource.pData, &pWicBitmap);
    if (FAILED(hr)) {
        goto done;
    }
    
    // 1. Create and init encoder
    hr = pWicFactory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &pEncoder);
    if (FAILED(hr)) {
        goto done;
    }

    // 2. Set up an output buffer
    hr = CreateStreamOnHGlobal(nullptr, TRUE, &pOutStream);
    if (FAILED(hr)) {
        goto done;
    }
    hr = pEncoder->Initialize(pOutStream, WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        goto done;
    }

    // 3. Create and init frame
    hr = pEncoder->CreateNewFrame(&pFrame, nullptr);
    if (FAILED(hr)) {
        goto done;
    }
    hr = pFrame->Initialize(nullptr);
    if (FAILED(hr)) {
        goto done;
    }
    hr = pFrame->SetSize(desc.Width, desc.Height);
    if (FAILED(hr)) {
        goto done;
    }
    WICPixelFormatGUID pixelFormatGuid = GUID_WICPixelFormat32bppRGBA;
    hr = pFrame->SetPixelFormat(&pixelFormatGuid);
    if (FAILED(hr)) {
        goto done;
    }

    // 4. Write to stream
    hr = pFrame->WriteSource(pWicBitmap, nullptr);
    if (FAILED(hr)) {
        goto done;
    }

    // 5. Commit changes
    hr = pFrame->Commit();
    if (FAILED(hr)) {
        goto done;
    }
    hr = pEncoder->Commit();
    if (FAILED(hr)) {
        goto done;
    }

    // 6. Get data from out stream
    HGLOBAL hGlobal = nullptr;
    GetHGlobalFromStream(pOutStream, &hGlobal);
    char* jpegData = (char*)GlobalLock(hGlobal);
    SIZE_T jpegSize = GlobalSize(hGlobal);

    // 7. Send data to client
    int bytesSent = send(clientSocket, (char*)&jpegSize, sizeof(int), 0);
    bytesSent = send(clientSocket, jpegData, (int)jpegSize, 0);
    if (bytesSent == 0 || bytesSent == SOCKET_ERROR) {
        DWORD err = WSAGetLastError();
        UNREFERENCED_PARAMETER(err);
        hr = E_FAIL;
    }
    
done:
    // --- END Create picture
    pDeviceContext->Unmap(pTexture, 0);

    CoSafeRelease(&pResource);
    CoSafeRelease(&pTexture);

    CoSafeRelease(&pWicFactory);
    CoSafeRelease(&pFrame);
    CoSafeRelease(&pEncoder);
    CoSafeRelease(&pOutStream);

    return hr;
}

void MonitorContext::FinalizeFrameSending() {
    if (clientSocket == INVALID_SOCKET) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }
}

#pragma endregion
