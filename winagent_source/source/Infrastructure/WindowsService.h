#ifndef WINDOWSSERVICE_H
#define WINDOWSSERVICE_H

#ifdef INFRASTRUCTURE_EXPORTS
#define INFRASTRUCTURE_API __declspec(dllexport)
#else
#define INFRASTRUCTURE_API __declspec(dllimport)
#endif

#include <windows.h>
#include <functional>

class INFRASTRUCTURE_API WindowsService {
public:
    // Define callback types for service handlers
    using ServiceStartCallback = std::function<void(DWORD argc, const wchar_t* const* argv)>;
    using ServiceStopCallback = std::function<void()>;
    
    // Methods to register callbacks
    static void RegisterStartHandler(ServiceStartCallback callback);
    static void RegisterStopHandler(ServiceStopCallback callback);

    // Install the service in the SCM
    static void InstallService();

    // Remove the service from the SCM
    static void RemoveService();

    // Run the service (dispatches to ServiceMain)
    static void RunService();

    // Called by the SCM as the service’s main entry point
    static void WINAPI ServiceMain(DWORD argc, const wchar_t* const* argv);

    // Called by the SCM when a control code is sent (e.g. stop, shutdown)
    static void WINAPI ServiceCtrl(DWORD ctrlCode);

    // Report the current service status to the Service Control Manager
    static bool ReportStatus(DWORD currentState, DWORD exitCode, DWORD waitHint);

    // Add our event source to the registry (for event logging)
    static void AddEventSource(const wchar_t* path);

    // Set and get the current error code for the service
    static void SetServiceError(DWORD error);
    static DWORD GetServiceError();

private:
    // Store callbacks
    static ServiceStartCallback startCallback;
    static ServiceStopCallback stopCallback;

    // Static variables used for service status reporting
    static SERVICE_STATUS serviceStatus;
    static SERVICE_STATUS_HANDLE serviceStatusHandle;
    static DWORD serviceError;

    // The service’s name (used both in SCM and in event logs)
    static const wchar_t* const ServiceName;
};

#endif // WINDOWSSERVICE_H
