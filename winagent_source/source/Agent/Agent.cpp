/*
SyslogAgent: a syslog agent for Windows
Copyright 2021 Logzilla Corp.
*/

#include "stdafx.h"
#include "Service.h"
#include "Options.h"
#include "Result.h"
#include "Logger.h"
#include "Util.h"
#include "WindowsService.h"
#include "WindowsEventLog.h"

#include <stdio.h>
#include "time.h"

using namespace Syslog_agent;



#define NTSL_PATH_LEN			1024
#define NTSL_SYS_LEN			256

static SERVICE_STATUS			service_status;
static SERVICE_STATUS_HANDLE	service_status_handle;
static DWORD					service_error;

static void service_install();
static void WINAPI service_main(DWORD argc, const wchar_t* const* argv);
static void service_remove();
static void WINAPI service_ctrl(DWORD ctrlCode);
static void service_start(DWORD argc, const wchar_t* const* argv);
static void service_stop();
static bool service_report_status(DWORD currentState, DWORD exitCode, DWORD waitHint);
static void service_addEventSource(const wchar_t* path);


static int run_as_console();

// Pure C function for structured exception handling
LONG WINAPI GlobalExceptionHandler(EXCEPTION_POINTERS* pExceptionInfo) {
    // Get the exception code
    DWORD exceptionCode = pExceptionInfo->ExceptionRecord->ExceptionCode;
    DWORD exceptionFlags = pExceptionInfo->ExceptionRecord->ExceptionFlags;
    LPVOID exceptionAddress = pExceptionInfo->ExceptionRecord->ExceptionAddress;
    
    // Try to log using printf as logging system might not be available
    fprintf(stderr, "UNHANDLED EXCEPTION: Code=0x%08X, Flags=0x%08X, Address=0x%p\n", 
            exceptionCode, exceptionFlags, exceptionAddress);
    
    // Create an emergency log file with the exception info
    HANDLE hFile = CreateFileA("syslogagent_crash.log", 
                           FILE_APPEND_DATA, 
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        char buffer[1024];
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_s(&tm_now, &now);
        
        int len = sprintf_s(buffer, "[%04d-%02d-%02d %02d:%02d:%02d] FATAL CRASH: Exception 0x%08X at address 0x%p\r\n", 
                        tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday, 
                        tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec,
                        exceptionCode, exceptionAddress);
        DWORD bytesWritten;
        WriteFile(hFile, buffer, len, &bytesWritten, NULL);
        
        // Additional information about the exception
        switch (exceptionCode) {
            case EXCEPTION_ACCESS_VIOLATION:
                len = sprintf_s(buffer, "ACCESS VIOLATION: %s operation at address 0x%p\r\n",
                            pExceptionInfo->ExceptionRecord->ExceptionInformation[0] ? "Write" : "Read",
                            (void*)pExceptionInfo->ExceptionRecord->ExceptionInformation[1]);
                break;
            case EXCEPTION_STACK_OVERFLOW:
                len = sprintf_s(buffer, "STACK OVERFLOW\r\n");
                break;
            case EXCEPTION_ILLEGAL_INSTRUCTION:
                len = sprintf_s(buffer, "ILLEGAL INSTRUCTION\r\n");
                break;
            case EXCEPTION_PRIV_INSTRUCTION:
                len = sprintf_s(buffer, "PRIVILEGED INSTRUCTION\r\n");
                break;
            default:
                len = sprintf_s(buffer, "EXCEPTION CODE: 0x%08X\r\n", exceptionCode);
                break;
        }
        WriteFile(hFile, buffer, len, &bytesWritten, NULL);
        
        // Register info
        CONTEXT* ctx = pExceptionInfo->ContextRecord;
        #ifdef _M_X64
        len = sprintf_s(buffer, "Registers: RAX=0x%016llX, RBX=0x%016llX, RCX=0x%016llX, RDX=0x%016llX\r\n", 
                    ctx->Rax, ctx->Rbx, ctx->Rcx, ctx->Rdx);
        WriteFile(hFile, buffer, len, &bytesWritten, NULL);
        len = sprintf_s(buffer, "          RSI=0x%016llX, RDI=0x%016llX, RBP=0x%016llX, RSP=0x%016llX\r\n", 
                    ctx->Rsi, ctx->Rdi, ctx->Rbp, ctx->Rsp);
        WriteFile(hFile, buffer, len, &bytesWritten, NULL);
        len = sprintf_s(buffer, "          RIP=0x%016llX\r\n", ctx->Rip);
        #else
        len = sprintf_s(buffer, "Registers: EAX=0x%08X, EBX=0x%08X, ECX=0x%08X, EDX=0x%08X\r\n", 
                    ctx->Eax, ctx->Ebx, ctx->Ecx, ctx->Edx);
        WriteFile(hFile, buffer, len, &bytesWritten, NULL);
        len = sprintf_s(buffer, "          ESI=0x%08X, EDI=0x%08X, EBP=0x%08X, ESP=0x%08X\r\n", 
                    ctx->Esi, ctx->Edi, ctx->Ebp, ctx->Esp);
        WriteFile(hFile, buffer, len, &bytesWritten, NULL);
        len = sprintf_s(buffer, "          EIP=0x%08X\r\n", ctx->Eip);
        #endif
        WriteFile(hFile, buffer, len, &bytesWritten, NULL);
        
        CloseHandle(hFile);
    }
    
    // Log to event log as a last resort
    HANDLE hEventLog = RegisterEventSourceA(NULL, "SyslogAgent");
    if (hEventLog) {
        char msg[256];
        sprintf_s(msg, "SyslogAgent crashed with exception 0x%08X at address 0x%p", 
                exceptionCode, exceptionAddress);
        const char* strings[] = { msg };
        ReportEventA(hEventLog, EVENTLOG_ERROR_TYPE, 0, 0, NULL, 1, 0, strings, NULL);
        DeregisterEventSource(hEventLog);
    }
    
    // Report the service stopped status to the service control manager.
    DWORD err = GetLastError();
    service_report_status(SERVICE_STOPPED, err, 0);
    
    return EXCEPTION_CONTINUE_SEARCH;
}

int wmain(int argc, wchar_t *argv[]) {
    shared_ptr<Logger> last_resort_logger = make_shared<Logger>(Logger::LAST_RESORT_LOGGER_NAME);
#if defined(_DEBUG) || defined(DEBUG) 
    std::string logFilePath = Util::getAppropriateLogPath("syslogagent_failsafe.log");
    last_resort_logger->setLogFile(logFilePath.c_str());
    last_resort_logger->setLogDestination(Logger::DEST_FILE);
    last_resort_logger->setCloseAfterWrite(true);
    
    // Log the location of the last resort log file to the Windows Event Log
    WindowsEventLog eventLog;
    eventLog.WriteEvent(
        WindowsEventLog::EventType::INFORMATION_EVENT,
        1000,  // Event ID
        "LogZilla SyslogAgent started",
        ("Last resort log file is located at: " + logFilePath).c_str());
#else
    last_resort_logger->setLogDestination(Logger::DEST_NONE);
#endif
    Logger::setLogger(last_resort_logger, { Logger::LAST_RESORT_LOGGER_NAME });
    LAST_RESORT_LOGGER->always("Starting SyslogAgent\n");
    SetUnhandledExceptionFilter(GlobalExceptionHandler);
    auto logger = LOG_THIS;

	LAST_RESORT_LOGGER->always("Registering service handlers\n");
    WindowsService::RegisterStartHandler([](DWORD argc, const wchar_t* const* argv) {
        auto logger = LOG_THIS;
        if (!WindowsService::ReportStatus(SERVICE_START_PENDING, NO_ERROR, 3000)) {
            logger->log(Logger::ALWAYS, "Failed to report start pending\n");
            LAST_RESORT_LOGGER->log(Logger::ALWAYS, "Failed to report start pending\n");
            return;
        }
        Registry::loadSetupFile();
        try {
            if (!WindowsService::ReportStatus(SERVICE_RUNNING, NO_ERROR, 0))
                return;
            Service::run(false);
        }
        catch (Result& exception) {
            WindowsService::ReportStatus(SERVICE_STOPPED, exception.statusCode(), 0);
            exception.log();
        }
        catch (std::exception& exception) {
            WindowsService::ReportStatus(SERVICE_STOPPED, 1, 0);
            logger->log(Logger::ALWAYS, "%s\n", exception.what());
            LAST_RESORT_LOGGER->log(Logger::ALWAYS, "%s\n", exception.what());
        }
        });

    WindowsService::RegisterStopHandler([]() {
        auto logger = LOG_THIS;
        logger->log(Logger::DEBUG, "AppServiceStop: service stop requested\n");
        WindowsService::ReportStatus(SERVICE_STOP_PENDING, NO_ERROR, 2500);
        Service::shutdown();
        });

    Options options(argc, const_cast<const wchar_t* const*>(argv));

    LAST_RESORT_LOGGER->log(Logger::ALWAYS, "Reading command line options\n");
    bool running_as_service = !options.has(L"-console");

    bool override_log_level = false;
    Logger::LogLevel override_log_level_setting = Logger::ALWAYS;
    if (options.has(L"-debug")) {
        override_log_level = true;
        override_log_level_setting = Logger::DEBUG;
    }
    else if (options.has(L"-debug2")) {
        override_log_level = true;
        override_log_level_setting = Logger::DEBUG2;
    }
    else if (options.has(L"-debug3")) {
        override_log_level = true;
        override_log_level_setting = Logger::DEBUG3;
    }


    LAST_RESORT_LOGGER->log(Logger::ALWAYS, "Loading configuration\n");
    Service::loadConfiguration(!running_as_service, override_log_level, override_log_level_setting);

    if (options.has(L"-tofile")) {
        logger->setLogDestination(Logger::DEST_CONSOLE_AND_FILE);
        const wchar_t* destination = options.getArgument(L"-tofile");
        if (destination != nullptr) {
            std::wstring destination_ws(destination);
            if (destination_ws[0] != L'-') {
                logger->setLogFileW(destination_ws);
            }
        }
    }

#ifdef THIS_IS_NOT_DISABLED
    if (options.has(L"-eventstofile")) {
        logger->setLogEventsToFile(true);
    }
#endif

    if (options.has(L"-version")) {
        printf("LogZilla Syslog Agent version %s.%s.%s.%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_FIXVERSION, VERSION_MINORFIXVERSION);
        return 0;
    }

    if (options.has(L"-install")) {
        WindowsService::InstallService();
        return 0;
    }

    if (options.has(L"-remove")) {
        WindowsService::RemoveService();
        return 0;
    }

    LAST_RESORT_LOGGER->log(Logger::ALWAYS, "Starting main process\n");

    if (!running_as_service) {
        LAST_RESORT_LOGGER->log(Logger::ALWAYS, "Starting on console\n");
        logger->always("%s starting on console. Version %s.%s.%s.%s\n", APP_NAME, VERSION_MAJOR, VERSION_MINOR, VERSION_FIXVERSION, VERSION_MINORFIXVERSION);
        return run_as_console();
    }
    else {
        LAST_RESORT_LOGGER->log(Logger::ALWAYS, "Starting as service\n");
        logger->setLogDestination(Logger::DEST_CONSOLE_AND_FILE);
        logger->always("%s starting as service. Version %s.%s.%s.%s\n", APP_NAME, VERSION_MAJOR, VERSION_MINOR, VERSION_FIXVERSION, VERSION_MINORFIXVERSION);
        WindowsService::RunService();
        LAST_RESORT_LOGGER->log(Logger::ALWAYS, "WindowsService::RunService done\n");
    }

    return 0;
}

// C-style helper to log a crash when running in console mode
void LogConsoleModeCrash() {
    HANDLE hFile = CreateFileA("syslogagent_crash.log", 
                           FILE_APPEND_DATA, 
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        const char* msg = "Application crashed in console mode\r\n";
        DWORD bytesWritten;
        WriteFile(hFile, msg, (DWORD)strlen(msg), &bytesWritten, NULL);
        CloseHandle(hFile);
    }
    // Report the service stopped status to the service control manager.
    DWORD err = GetLastError();
    service_report_status(SERVICE_STOPPED, err, 0);
}

static int run_as_console() {
    auto logger = LOG_THIS;
    try {
        Service::run(true);
    }
    catch (std::exception& exception) {
        logger->log(Logger::CRITICAL, "%s\n", exception.what());
        return 1;
    }
    return 0;
}

/*-----------------------------[ service_install ]-----------------------------
*  Installs the service.
*----------------------------------------------------------------------------*/
static void service_install()
{
    auto logger = LOG_THIS;
    SC_HANDLE   service;
    SC_HANDLE   manager;
    wchar_t		path[NTSL_PATH_LEN];
    LPCTSTR lpDependencies = __TEXT("EventLog\0");
    HKEY hk;
    //    DWORD dwData; 
    wchar_t serviceDescription[] = L"Forwards Event logs to the Syslog server";

    if (GetModuleFileName(NULL, path + 1, NTSL_PATH_LEN - 1) == 0)
    {
        Result::logLastError("service_install()", "GetModuleFileName");
        logger->log(Logger::CRITICAL, "Unable to install %ls\n", SERVICE_NAME);
        return;
    }

    // quote path
    path[0] = '"';
    wcscat_s(path, NTSL_PATH_LEN, L"\"");

    service_addEventSource(path);

    manager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (manager)
    {
        service = CreateService(
            manager,                    // SCManager database
            SERVICE_NAME,         // name of service
            SERVICE_NAME,			// name to display
            SERVICE_ALL_ACCESS,         // desired access
            SERVICE_WIN32_OWN_PROCESS,  // service type
            SERVICE_AUTO_START,			// start type
            SERVICE_ERROR_NORMAL,       // error control type
            path,                       // service's binary
            NULL,                       // no load ordering group
            NULL,                       // no tag identifier
            lpDependencies,             // dependencies
            NULL,                       // LocalSystem account
            NULL);                      // no password

        if (service)
        {
            logger->log(Logger::INFO, "%ls installed\n", SERVICE_NAME);
            CloseServiceHandle(service);
        }
        else
        {
            Result::logLastError("service_install()", "CreateService");
        }

        CloseServiceHandle(manager);
    }
    else {
        Result::logLastError("service_install()", "OpenSCManager");
        return;
    }

    if (RegOpenKey(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\LZ Syslog Agent", &hk)) {
        Result::logLastError("service_install()", "RegOpenKey");
        //fail - no big deal
        return;
    }

    // Add the name to the EventMessageFile subkey. 
    if (RegSetValueEx(hk,             // subkey handle 
        L"Description",		       // value name 
        0,                        // must be zero 
        REG_EXPAND_SZ,            // value type 
        (LPBYTE)serviceDescription,           // pointer to value data 
        (DWORD) (wcslen(serviceDescription) + 1)*sizeof(wchar_t))) {       // length of value data 
        Result::logLastError("service_install()", "RegSetValueEx");
        return; //error - no big deal
    }

    RegCloseKey(hk);

}

/*-----------------------------[ service_remove ]-----------------------------
*  Stops and removes the service
*----------------------------------------------------------------------------*/
static void service_remove()
{
    auto logger = LOG_THIS;
    SC_HANDLE   service;
    SC_HANDLE   manager;

    manager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (manager)
    {
        service = OpenService(manager, SERVICE_NAME, SERVICE_ALL_ACCESS);

        if (service)
        {
            // try to stop the service
            if (ControlService(service, SERVICE_CONTROL_STOP, &service_status))
            {
                logger->log(Logger::INFO, "Stopping %ls\n", SERVICE_NAME);
                Sleep(1000);

                while (QueryServiceStatus(service, &service_status))
                {
                    if (service_status.dwCurrentState == SERVICE_STOP_PENDING)
                    {
                        printf(".");
                        Sleep(1000);
                    }
                    else
                        break;
                }

                if (service_status.dwCurrentState == SERVICE_STOPPED)
                    logger->log(Logger::INFO, "%ls stopped.\n", SERVICE_NAME);
                else
                    logger->log(Logger::CRITICAL, "%ls failed to stop\n", SERVICE_NAME);
            }

            // now remove the service
            if (DeleteService(service))
                logger->log(Logger::INFO, "%ls removed\n", SERVICE_NAME);
            else
                Result::logLastError("service_remove()", "DeleteService");


            CloseServiceHandle(service);
        }
        else
            Result::logLastError("service_remove()", "OpenService");

        CloseServiceHandle(manager);
    }
    else
        Result::logLastError("service_remove()", "OpenSCManager");
}

/*------------------------------[ service_main ]------------------------------
*  Calls service initialization routines.
*
*  Parameters:
*		argc  -	 number of command line arguments
*		argv  -  array of command line arguments
*
*  Return value:
*		none
*----------------------------------------------------------------------------*/
static void WINAPI service_main(DWORD argc, const wchar_t* const* argv) {

    auto logger = LOG_THIS;
    logger->info("Start service %s Version %s.%s.%s.%s\n", APP_NAME, VERSION_MAJOR, VERSION_MINOR, VERSION_FIXVERSION, VERSION_MINORFIXVERSION);

    service_status_handle = RegisterServiceCtrlHandler(SERVICE_NAME, service_ctrl);

    if (!service_status_handle) {
        Result::logLastError("service_main()", "RegisterServiceCtrlHandler");
    }

    else {
        service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        service_status.dwServiceSpecificExitCode = 0;


        // report the status to the service control manager.
        if (service_report_status(SERVICE_START_PENDING, NO_ERROR, 3000)) {
            service_start(argc, argv);
        }
    }

    // try to report the stopped status to the service control manager.
    if (service_status_handle) {
        logger->log(Logger::DEBUG, "Leaving service_main, and reporting service stopped.\n");
        service_report_status(SERVICE_STOPPED, service_error, 0);
    }
}

/*------------------------------[ service_ctrl ]------------------------------
*  Called by the SCM whenever ControlService() is called for this service
*
*  Parameters:
*		ctrlCode - type of control requested
*
*  Return value:
*		none
*----------------------------------------------------------------------------*/
void WINAPI service_ctrl(DWORD ctrlCode) {

    auto logger = LOG_THIS;
    logger->log(Logger::DEBUG, "Service_ctrl received code %u.\n", ctrlCode);

    switch (ctrlCode) {
        // stop the service.
        //
        // SERVICE_STOP_PENDING should be reported before
        // setting the Stop Event - hServerStopEvent - in
        // service_stop().  This avoids a race condition
        // which may result in a 1053 - The Service did not respond...
        // error.
    case SERVICE_CONTROL_SHUTDOWN:
    case SERVICE_CONTROL_STOP:
        service_stop();
        return;

        // Update the service status.
        //
    case SERVICE_CONTROL_INTERROGATE:
        break;

        // invalid control code
        //
    default:
        break;
    }

    service_report_status(service_status.dwCurrentState, NO_ERROR, 0);
}

/*------------------------------[ service_start ]------------------------------
* Starts and runs the service
*----------------------------------------------------------------------------*/
void service_start(DWORD argc, const wchar_t* const* argv) {

    auto logger = LOG_THIS;
    // report the status to the service control manager.
    if (!service_report_status(SERVICE_START_PENDING, NO_ERROR, 3000)) {
        logger->log(Logger::ALWAYS, "Failed to report start pending to service handler from service_start.\n");
        return;
    }

    // if there's a new setup file, read it
    Registry::loadSetupFile();

    try {
        // report the status to the service control manager.
        if (!service_report_status(SERVICE_RUNNING, NO_ERROR, 0))
            return;
        //Service::loadConfiguration(false, false, Logger::LogLevel::ALWAYS);
        Service::run(false);
    }
    catch (Result& exception) {
        DWORD err = exception.statusCode();
        service_report_status(SERVICE_STOPPED, err, 0);
        exception.log();
    }
    catch (std::exception& exception) {
        DWORD err = GetLastError();
        service_report_status(SERVICE_STOPPED, err, 0);
        logger->log(Logger::ALWAYS, "%s\n", exception.what());
    }

}

/*------------------------------[ service_stop ]------------------------------
* Stops the service.
*
* NOTE: If this service will take longer than 3 seconds,
* spawn a thread to execute the stop code and return.
* Otherwise the SCM will think the service has stopped responding.
*----------------------------------------------------------------------------*/
void service_stop()
{
    auto logger = LOG_THIS;
    logger->log(Logger::DEBUG, "Registered service_stop_event\n");
    service_report_status(SERVICE_STOP_PENDING, NO_ERROR, 2500);
    Service::shutdown();
}

/*--------------------------[ service_report_status ]--------------------------
*  Sets the current status and reports it to the Service Control Manager
*
*  Parameters:
*		currentState	-  the state of the service
*		exitCode		-  error code to report
*		waitHint		-  worst case estimate to next checkpoint
*
*  Return value:
*		true			-  success
*		false			-  failure
*----------------------------------------------------------------------------*/
static bool service_report_status(DWORD currentState, DWORD exitCode, DWORD waitHint) {
    static DWORD checkPoint = 1;

    if (currentState == SERVICE_START_PENDING)
        service_status.dwControlsAccepted = 0;
    else
        service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

    service_status.dwCurrentState = currentState;
    service_status.dwWin32ExitCode = exitCode;
    service_status.dwWaitHint = waitHint;

    if ((currentState == SERVICE_RUNNING) ||
        (currentState == SERVICE_STOPPED))
        service_status.dwCheckPoint = 0;
    else
        service_status.dwCheckPoint = checkPoint++;

    // report the status of the service to the service control manager.
    if (!SetServiceStatus(service_status_handle, &service_status)) {
        Result::logLastError("service_report_status()", "SetServiceStatus");
        return false;
    }
    return true;
}


void service_addEventSource(const wchar_t* path)
{
    auto logger = LOG_THIS;
    HKEY hk;
    DWORD dwData;

    if (path == NULL) return;

    // Add your source name as a subkey under the Application 
    // key_ in the EventLog registry key_. 

    if (RegCreateKey(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\LZ Syslog Agent", &hk)) {
        Result::logLastError("service_addEventSource()", "RegCreateKey");
        return;
    }


    // Add the name to the EventMessageFile subkey. 
    if (RegSetValueEx(hk,             // subkey handle 
        L"EventMessageFile",       // value name 
        0,                        // must be zero 
        REG_EXPAND_SZ,            // value type 
        (LPBYTE)path,           // pointer to value data 
        (DWORD)(wcslen(path) + 1) * sizeof(wchar_t))) {       // length of value data 
        Result::logLastError("service_addEventSource()", "RegSetValueEx");
        RegCloseKey(hk);
        return;
    }

    // Set the supported event types in the TypesSupported subkey. 

    dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE |
        EVENTLOG_INFORMATION_TYPE;

    if (RegSetValueEx(hk,      // subkey handle 
        L"TypesSupported",  // value name 
        0,                 // must be zero 
        REG_DWORD,         // value type 
        (LPBYTE)&dwData,  // pointer to value data 
        sizeof(DWORD))) {   // length of value data 
        Result::logLastError("service_addEventSource()", "RegSetValueEx");
    }

    RegCloseKey(hk);
    logger->log(Logger::DEBUG, "Added event source\n");
}
