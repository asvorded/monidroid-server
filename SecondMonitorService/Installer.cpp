#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <strsafe.h>
#include <winsock2.h>

#include "Installer.h"
#include "ErrorHandling.h"

#include "../MonidroidInfo/Monidroid.h"

int InstallService() {
    WCHAR szUnquotedPath[MAX_PATH];

    if (!GetModuleFileNameW(NULL, szUnquotedPath, MAX_PATH)) {
        printf("Cannot install service.\n");
        ConsoleSystemErrorCodeHandler(GetLastError());
        return -1;
    }

    WCHAR szPath[MAX_PATH];
    StringCbPrintfW(szPath, MAX_PATH, L"\"%s\"", szUnquotedPath);

	SC_HANDLE hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

    SC_HANDLE schService = CreateServiceW(
        hSCM,                           // SCM database 
        MY_SERVICE_NAME,                // name of service 
        MY_SERVICE_NAME,                // service name to display 
        SERVICE_ALL_ACCESS,             // desired access 
        SERVICE_WIN32_OWN_PROCESS,      // service type 
        SERVICE_AUTO_START,             // start type 
        SERVICE_ERROR_NORMAL,           // error control type 
        szPath,                         // path to service's binary 
        NULL,                           // no load ordering group 
        NULL,                           // no tag identifier 
        NULL,                           // no dependencies 
        NULL,                           // LocalSystem account 
        NULL);                          // no password

    if (schService == NULL) {
        printf("Install failed.\n");
        ConsoleSystemErrorCodeHandler(GetLastError());
        return -1;
    }

    printf("Service installed successfully.\n");
	return 0;
}

int UninstallService() {
    SC_HANDLE hSCM;
    SC_HANDLE schService;

    // Get a handle to the SCM database. 
    hSCM = OpenSCManagerW(
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 

    if (NULL == hSCM) {
        printf("Cannot delete the service.\n");
        ConsoleSystemErrorCodeHandler(GetLastError());
        return -1;
    }

    // Get a handle to the service.
    schService = OpenServiceW(
        hSCM,               // SCM database 
        MY_SERVICE_NAME,    // name of service 
        DELETE);            // need delete access 

    if (schService == NULL) {
        printf("Cannot delete the service.\n");
        ConsoleSystemErrorCodeHandler(GetLastError());
        CloseServiceHandle(hSCM);
        return -1;
    }

    // Delete the service.
    if (!DeleteService(schService)) {
        printf("Delete failed.\n");
        ConsoleSystemErrorCodeHandler(GetLastError());
    } else {
        printf("Service uninstalled successfully.\n");
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(hSCM);

    return 0;
}