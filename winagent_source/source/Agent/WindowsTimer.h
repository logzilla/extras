/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once
#include <Windows.h>
#include <string>
#include <synchapi.h>

using namespace std;

class WindowsTimer {
public:
	WindowsTimer();
	~WindowsTimer();
	bool wait(int milliseconds);
	void set(int milliseconds);
	void reset();
	void close();

private:
	HANDLE windows_timer_;
	bool is_timer_running_;
};