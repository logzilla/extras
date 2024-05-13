/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once
#include <Windows.h>
#include <string>
#include <synchapi.h>

using namespace std;

class WindowsEvent {
public:
	WindowsEvent(wstring event_name);
	~WindowsEvent();
	bool wait(int milliseconds);
	void signal();
	void reset();
	void close();

private:
	wstring event_name_;
	HANDLE windows_event_;
};