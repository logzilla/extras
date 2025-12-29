/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once
#include <Windows.h>
#include <string>
#include <synchapi.h>

using namespace std;

#ifdef WINEVENT_LIB
    #define WINEVENT_API __declspec(dllexport)
#else
    #define WINEVENT_API __declspec(dllimport)
#endif

#ifdef INFRASTRUCTURE_EXPORTS
    #define INFRASTRUCTURE_API __declspec(dllexport)
#else
    #define INFRASTRUCTURE_API __declspec(dllimport)
#endif


class INFRASTRUCTURE_API WindowsEvent {
public:
	WindowsEvent(const std::wstring& name);
	~WindowsEvent();
	bool wait(DWORD timeout_ms = INFINITE);
	void signal();
	void reset();
	void close();

private:
	wstring event_name_;
	HANDLE handle_;
};
