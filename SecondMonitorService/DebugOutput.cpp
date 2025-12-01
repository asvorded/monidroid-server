#include "DebugOutput.h"

#include <Windows.h>
#include <stdio.h>

void DebugPrint(const wchar_t* format, ...) {
    wchar_t buffer[1024];
    va_list args;
    va_start(args, format);
    vswprintf(buffer, 1024, format, args);
    va_end(args);
    OutputDebugStringW(buffer);
}

void ConsoleSystemErrorCodeHandler(DWORD code) {
	char* error = NULL;
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL, code, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), (char*)&error, 0, NULL);

	printf("Error message: %s\n", error);

	LocalFree(error);
}
