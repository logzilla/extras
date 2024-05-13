/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#include "stdafx.h"
#include "Service.h"
#include "Options.h"
#include "Result.h"
#include "Logger.h"
#include "Util.h"

using namespace Syslog_agent;

#define NTSL_PATH_LEN			1024
#define NTSL_SYS_LEN			256

static SERVICE_STATUS			service_status;
static SERVICE_STATUS_HANDLE	service_status_handle;
static DWORD					service_error;

static void service_install();
static void WINAPI service_main(DWORD argc, wchar_t **argv);
static void service_remove();
static void WINAPI service_ctrl(DWORD ctrlCode);
static void service_start(DWORD argc, wchar_t **argv);
static void service_stop();
static bool service_report_status(DWORD currentState, DWORD exitCode, DWORD waitHint);
static void service_addEventSource(const wchar_t* path);


static int run_as_console();


int wmain(int argc, wchar_t *argv[]) {

    Options options(argc, argv);

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

    Service::loadConfiguration(!running_as_service, override_log_level, override_log_level_setting);

    if (options.has(L"-tofile")) {
        Logger::setLogDestination(Logger::DEST_CONSOLE_AND_FILE);
        wchar_t* destination = options.getArgument(L"-tofile");
        if (destination != nullptr) {
            std::wstring destination_ws(destination);
            if (destination_ws[0] != L'-') {
                Logger::setLogFile(destination_ws);
            }
        }
    }

    if (options.has(L"-eventstofile")) {
        Logger::setLogEventsToFile();
    }

    if (options.has(L"-version")) {
        printf("LogZilla Syslog Agent version %s.%s.%s.%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_FIXVERSION, VERSION_MINORFIXVERSION);
    }

    if (options.has(L"-install")) {
        service_install();
        return 0;
    }

    if (options.has(L"-remove")) {
        service_remove();
        return 0;
    }


    if (!running_as_service) {
        Logger::always("%s starting on console. Version %s.%s.%s.%s\n", APP_NAME, VERSION_MAJOR, VERSION_MINOR, VERSION_FIXVERSION, VERSION_MINORFIXVERSION);
        return run_as_console();
    }
    else {
        Logger::always("%s starting as service. Version %s.%s.%s.%s\n", APP_NAME, VERSION_MAJOR, VERSION_MINOR, VERSION_FIXVERSION, VERSION_MINORFIXVERSION);
    }

    SERVICE_TABLE_ENTRY dispatchTable[] = {
        { SERVICE_NAME, service_main },
        { NULL, NULL }
    };
    if (!StartServiceCtrlDispatcher(dispatchTable)) {
        Result result(GetLastError(), "wmain()", "StartServiceCtrlDispatcher");
        if (result.statusCode() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            return run_as_console();
        }
        result.log();
    }

    return 0;
}

static int run_as_console() {
    try {
        Service().run(true);
    }
    catch (std::exception& exception) {
        Logger::log(Logger::CRITICAL, "%s\n", exception.what());
        return 1;
    }
    return 0;
}

/*-----------------------------[ service_install ]-----------------------------
*  Installs the service.
*----------------------------------------------------------------------------*/
static void service_install()
{
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
        Logger::log(Logger::CRITICAL, "Unable to install %ls\n", SERVICE_NAME);
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
            Logger::log(Logger::INFO, "%ls installed\n", SERVICE_NAME);
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
                Logger::log(Logger::INFO, "Stopping %ls\n", SERVICE_NAME);
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
                    Logger::log(Logger::INFO, "%ls stopped.\n", SERVICE_NAME);
                else
                    Logger::log(Logger::CRITICAL, "%ls failed to stop\n", SERVICE_NAME);
            }

            // now remove the service
            if (DeleteService(service))
                Logger::log(Logger::INFO, "%ls removed\n", SERVICE_NAME);
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
static void WINAPI service_main(DWORD argc, wchar_t **argv) {

    Logger::info("Start service %s Version %s.%s.%s.%s\n", APP_NAME, VERSION_MAJOR, VERSION_MINOR, VERSION_FIXVERSION, VERSION_MINORFIXVERSION);

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
        Logger::log(Logger::DEBUG, "Leaving service_main, and reporting service stopped.\n");
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

    Logger::log(Logger::DEBUG, "Service_ctrl received code %u.\n", ctrlCode);

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
void service_start(DWORD argc, wchar_t **argv) {

    // report the status to the service control manager.
    if (!service_report_status(SERVICE_START_PENDING, NO_ERROR, 3000)) {
        Logger::log(Logger::ALWAYS, "Failed to report start pending to service handler from service_start.\n");
        return;
    }

    // if there's a new setup file, read it
    Registry::loadSetupFile();

    try {
        // report the status to the service control manager.
        if (!service_report_status(SERVICE_RUNNING, NO_ERROR, 0))
            return;
        //Service::loadConfiguration(false, false, Logger::LogLevel::ALWAYS);
        Service().run(false);
    }
    catch (Result& exception) {
        service_report_status(SERVICE_STOPPED, exception.statusCode(), 0);
        exception.log();
    }
    catch (std::exception& exception) {
        service_report_status(SERVICE_STOPPED, 1, 0);
        Logger::log(Logger::ALWAYS, "%s\n", exception.what());
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
    Logger::log(Logger::DEBUG, "Registered service_stop_event\n");
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
        (LPBYTE)path + sizeof(wchar_t),           // pointer to value data 
        (DWORD) (wcslen(path) - 2)*sizeof(wchar_t))) {       // length of value data 
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
}
