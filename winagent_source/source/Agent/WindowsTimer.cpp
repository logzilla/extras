/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#include "stdafx.h"
#include "WindowsTimer.h"

using namespace std;

WindowsTimer::WindowsTimer() : is_timer_running_(false)  {
	windows_timer_ = CreateWaitableTimer(NULL, true, NULL);
}

bool WindowsTimer::wait(int milliseconds) {
    DWORD result = WaitForSingleObject(windows_timer_, (DWORD)milliseconds);

    switch (result) {
    case WAIT_OBJECT_0:
        // The timer was signaled.
        return true;
    case WAIT_TIMEOUT:
        // The wait operation timed out.
        return false;
    default:
        // An error occurred.
        return false;
    }
}


void WindowsTimer::reset() {
    if (is_timer_running_) {
        CancelWaitableTimer(windows_timer_);
        is_timer_running_ = false;
    }
}

void WindowsTimer::set(int milliseconds) {
	if (!is_timer_running_) {
        LARGE_INTEGER liDueTime;
		liDueTime.QuadPart = -10000 * milliseconds;
		SetWaitableTimer(windows_timer_, &liDueTime, 0, NULL, NULL, FALSE);
		is_timer_running_ = true;
    }
}

void WindowsTimer::close() {
	CloseHandle(windows_timer_);
}

WindowsTimer::~WindowsTimer() {
	close();
}

