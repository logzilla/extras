/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#include "stdafx.h"
#include "WindowsEvent.h"

using namespace std;

WindowsEvent::WindowsEvent(wstring event_name) {
	event_name_ = event_name;
	windows_event_ = CreateEvent(NULL, true, false, event_name.c_str());
}

bool WindowsEvent::wait(int milliseconds) {
	DWORD result = WaitForSingleObject(windows_event_, (DWORD)milliseconds);
	if (result == WAIT_OBJECT_0) {
		return true;
	}
	return false;
}

void WindowsEvent::reset() {
	ResetEvent(windows_event_);
}

void WindowsEvent::signal() {
	SetEvent(windows_event_);
}

void WindowsEvent::close() {
	CloseHandle(windows_event_);
}

WindowsEvent::~WindowsEvent() {
	close();
}

