#pragma once

#include <windows.h>

void DebugPrint(const wchar_t* format, ...);

void ConsoleSystemErrorCodeHandler(DWORD code);
