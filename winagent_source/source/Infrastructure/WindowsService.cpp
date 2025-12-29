#include "pch.h"
#include "WindowsService.h"
#include "Result.h"
#include "Logger.h"
#include "Util.h"

// Declare external functions at global scope
extern "C" void AppServiceStart(DWORD argc, const wchar_t* const* argv);
extern "C" void AppServiceStop();

// Provide weak stub implementations that will be overridden by actual implementations
// in the consuming application
#ifdef _MSC_VER
#pragma comment(linker, "/alternatename:AppServiceStart=__AppServiceStart_Stub")
#pragma comment(linker, "/alternatename:AppServiceStop=__AppServiceStop_Stub")
extern "C" void __AppServiceStart_Stub(DWORD, const wchar_t* const*) {}
extern "C" void __AppServiceStop_Stub() {}
#else
extern "C" __attribute__((weak)) void AppServiceStart(DWORD, const wchar_t* const*) {}
extern "C" __attribute__((weak)) void AppServiceStop() {}
#endif

// Define static members
SERVICE_STATUS WindowsService::serviceStatus = {0};
SERVICE_STATUS_HANDLE WindowsService::serviceStatusHandle = nullptr;
DWORD WindowsService::serviceError = 0;
const wchar_t* const WindowsService::ServiceName = L"LZ Syslog Agent";
WindowsService::ServiceStartCallback WindowsService::startCallback = nullptr;
WindowsService::ServiceStopCallback WindowsService::stopCallback = nullptr;

void WindowsService::RegisterStartHandler(ServiceStartCallback callback) {
    startCallback = callback;
}

void WindowsService::RegisterStopHandler(ServiceStopCallback callback) {
    stopCallback = callback;
}

void WindowsService::InstallService() {
    auto logger = LOG_THIS;
    SC_HANDLE service;
    SC_HANDLE manager;
    wchar_t path[1024];
    LPCTSTR lpDependencies = __TEXT("EventLog\0");
    HKEY hk;
    wchar_t serviceDescription[] = L"Forwards Event logs to the Syslog server";

    if (GetModuleFileName(NULL, path + 1, 1023) == 0) {
        Result::logLastError("InstallService()", "GetModuleFileName");
        logger->log(Logger::CRITICAL, "Unable to install %ls\n", ServiceName);
        return;
    }
    // Quote the path
    path[0] = '"';
    wcscat_s(path, 1024, L"\"");

    AddEventSource(path);

    manager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (manager) {
        service = CreateService(
            manager,
            ServiceName,
            ServiceName,
            SERVICE_ALL_ACCESS,
            SERVICE_WIN32_OWN_PROCESS,
            SERVICE_AUTO_START,
            SERVICE_ERROR_NORMAL,
            path,
            NULL,
            NULL,
            lpDependencies,
            NULL,
            NULL
        );

        if (service) {
            logger->log(Logger::INFO, "%ls installed\n", ServiceName);
            CloseServiceHandle(service);
        } else {
            Result::logLastError("InstallService()", "CreateService");
        }
        CloseServiceHandle(manager);
    } else {
        Result::logLastError("InstallService()", "OpenSCManager");
        return;
    }

    LONG regResult = RegOpenKey(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\LZ Syslog Agent", &hk);
    if (regResult != ERROR_SUCCESS) {
        Result::logLastError("InstallService()", "RegOpenKey");
        return;
    }

    if (RegSetValueEx(hk, L"Description", 0, REG_EXPAND_SZ,
        (LPBYTE)serviceDescription, (DWORD)(wcslen(serviceDescription) + 1) * sizeof(wchar_t))) {
        Result::logLastError("InstallService()", "RegSetValueEx");
        RegCloseKey(hk);
        return;
    }
    RegCloseKey(hk);
}

void WindowsService::RemoveService() {
    auto logger = LOG_THIS;
    SC_HANDLE service;
    SC_HANDLE manager;
    manager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (manager) {
        service = OpenService(manager, ServiceName, SERVICE_ALL_ACCESS);
        if (service) {
            if (ControlService(service, SERVICE_CONTROL_STOP, &serviceStatus)) {
                logger->log(Logger::INFO, "Stopping %ls\n", ServiceName);
                Sleep(1000);
                while (QueryServiceStatus(service, &serviceStatus)) {
                    if (serviceStatus.dwCurrentState == SERVICE_STOP_PENDING) {
                        printf(".");
                        Sleep(1000);
                    } else {
                        break;
                    }
                }
                if (serviceStatus.dwCurrentState == SERVICE_STOPPED)
                    logger->log(Logger::INFO, "%ls stopped.\n", ServiceName);
                else
                    logger->log(Logger::CRITICAL, "%ls failed to stop\n", ServiceName);
            }
            if (DeleteService(service))
                logger->log(Logger::INFO, "%ls removed\n", ServiceName);
            else
                Result::logLastError("RemoveService()", "DeleteService");

            CloseServiceHandle(service);
        } else {
            Result::logLastError("RemoveService()", "OpenService");
        }
        CloseServiceHandle(manager);
    } else {
        Result::logLastError("RemoveService()", "OpenSCManager");
    }
}

void WindowsService::RunService() {
    wchar_t serviceNameBuffer[256];
    wcscpy_s(serviceNameBuffer, ServiceName);
    SERVICE_TABLE_ENTRY dispatchTable[] = {
        { serviceNameBuffer, reinterpret_cast<LPSERVICE_MAIN_FUNCTION>(ServiceMain) },
        { NULL, NULL }
    };
    if (!StartServiceCtrlDispatcher(dispatchTable)) {
        Result result(GetLastError(), "RunService()", "StartServiceCtrlDispatcher");
        if (result.statusCode() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            // Possibly running as console – do nothing here.
            return;
        }
        result.log();
    }
}

void WINAPI WindowsService::ServiceMain(DWORD argc, const wchar_t* const* argv) {
    auto logger = LOG_THIS;
    logger->info("Start service %ls\n", ServiceName);
    serviceStatusHandle = RegisterServiceCtrlHandler(ServiceName, ServiceCtrl);
    if (!serviceStatusHandle) {
        Result::logLastError("ServiceMain()", "RegisterServiceCtrlHandler");
    } else {
        serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        serviceStatus.dwServiceSpecificExitCode = 0;
        if (ReportStatus(SERVICE_START_PENDING, NO_ERROR, 3000)) {
            // Call the registered callback first if available
            if (startCallback) {
                logger->debug("Calling registered start handler\n");
                startCallback(argc, argv);
            } else {
                // Fall back to the legacy C function for backward compatibility
                logger->debug("No registered start handler, falling back to AppServiceStart\n");
                AppServiceStart(argc, argv);
            }
        }
    }
    if (serviceStatusHandle) {
        logger->log(Logger::DEBUG, "Leaving ServiceMain, reporting service stopped.\n");
        ReportStatus(SERVICE_STOPPED, serviceError, 0);
    }
}

void WINAPI WindowsService::ServiceCtrl(DWORD ctrlCode) {
    auto logger = LOG_THIS;
    logger->log(Logger::DEBUG, "ServiceCtrl received code %u.\n", ctrlCode);
    switch (ctrlCode) {
        case SERVICE_CONTROL_SHUTDOWN:
        case SERVICE_CONTROL_STOP:
            // Call the registered callback first if available
            if (stopCallback) {
                logger->debug("Calling registered stop handler\n");
                stopCallback();
            } else {
                // Fall back to the legacy C function for backward compatibility
                logger->debug("No registered stop handler, falling back to AppServiceStop\n");
                AppServiceStop();
            }
            return;
        case SERVICE_CONTROL_INTERROGATE:
            break;
        default:
            break;
    }
    ReportStatus(serviceStatus.dwCurrentState, NO_ERROR, 0);
}

bool WindowsService::ReportStatus(DWORD currentState, DWORD exitCode, DWORD waitHint) {
    static DWORD checkPoint = 1;
    if (currentState == SERVICE_START_PENDING)
        serviceStatus.dwControlsAccepted = 0;
    else
        serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    serviceStatus.dwCurrentState = currentState;
    serviceStatus.dwWin32ExitCode = exitCode;
    serviceStatus.dwWaitHint = waitHint;
    if ((currentState == SERVICE_RUNNING) || (currentState == SERVICE_STOPPED))
        serviceStatus.dwCheckPoint = 0;
    else
        serviceStatus.dwCheckPoint = checkPoint++;
    if (!SetServiceStatus(serviceStatusHandle, &serviceStatus)) {
        Result::logLastError("ReportStatus()", "SetServiceStatus");
        return false;
    }
    return true;
}

void WindowsService::AddEventSource(const wchar_t* path) {
    auto logger = LOG_THIS;
    HKEY hk;
    DWORD dwData;
    if (path == NULL) return;
    if (RegCreateKey(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\LZ Syslog Agent", &hk)) {
        Result::logLastError("AddEventSource()", "RegCreateKey");
        return;
    }
    if (RegSetValueEx(hk, L"EventMessageFile", 0, REG_EXPAND_SZ, (LPBYTE)path, (DWORD)(wcslen(path) + 1) * sizeof(wchar_t))) {
        Result::logLastError("AddEventSource()", "RegSetValueEx");
        RegCloseKey(hk);
        return;
    }
    dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
    if (RegSetValueEx(hk, L"TypesSupported", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD))) {
        Result::logLastError("AddEventSource()", "RegSetValueEx");
    }
    RegCloseKey(hk);
    logger->log(Logger::DEBUG, "Added event source\n");
}

void WindowsService::SetServiceError(DWORD error) {
    serviceError = error;
}

DWORD WindowsService::GetServiceError() {
    return serviceError;
}
