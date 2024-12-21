#pragma once

#include <windows.h>
#include <SetupAPI.h>
#include <swdevice.h>
#include <devguid.h>
#include <winioctl.h>
#include <cstdio>

DWORD MonidroidInitGraphicsAdapter(HANDLE* pHandle);

DWORD MonidroidDestroyGraphicsAdapter();