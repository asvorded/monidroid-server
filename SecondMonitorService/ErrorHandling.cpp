#include "ErrorHandling.h"

void ConsoleSystemErrorCodeHandler(DWORD code) {
	char* error = NULL;
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL, code, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), (char*)&error, 0, NULL);

	printf("Error message: %s\n", error);

	LocalFree(error);
}
