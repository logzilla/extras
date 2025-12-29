/*
SyslogAgent: a syslog agent for Windows
Copyright 2021 Logzilla Corp.
*/

#include "pch.h"
#include <stdexcept>
#include "WindowsEvent.h"


WindowsEvent::WindowsEvent(const std::wstring& name) {
	handle_ = CreateEvent(nullptr, TRUE, FALSE, name.c_str());
	
	if (handle_ == NULL) {
		throw std::runtime_error("Failed to create Windows event");
	}
}

WindowsEvent::~WindowsEvent() {
	close();
}

void WindowsEvent::signal() {
	if (handle_) {
		SetEvent(handle_);
	}
}

void WindowsEvent::reset() {
	if (handle_) {
		ResetEvent(handle_);
	}
}

bool WindowsEvent::wait(DWORD timeout_ms) {
	if (handle_ == NULL) {
		return false;
	}
	
	DWORD result = WaitForSingleObject(handle_, timeout_ms);
	return result == WAIT_OBJECT_0;
}

void WindowsEvent::close() {
	if (handle_) {
		CloseHandle(handle_);
		handle_ = NULL;
	}
}
