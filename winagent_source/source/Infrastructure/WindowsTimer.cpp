#include "pch.h"
#include "WindowsTimer.h"
#include <stdexcept>

WindowsTimer::WindowsTimer() : windows_timer_(NULL), is_timer_running_(false) {
}

WindowsTimer::~WindowsTimer() {
	close();
}

void WindowsTimer::startTimer(DWORD milliseconds) {
	if (windows_timer_ != NULL) {
		close();
	}
	windows_timer_ = CreateWaitableTimer(NULL, TRUE, NULL);
	if (windows_timer_ == NULL) {
		throw std::runtime_error("Failed to create Windows timer");
	}
	LARGE_INTEGER due_time;
	due_time.QuadPart = -10000LL * milliseconds;
	SetWaitableTimer(windows_timer_, &due_time, 0, NULL, NULL, FALSE);
	is_timer_running_ = true;
}

void WindowsTimer::stopTimer() {
	if (windows_timer_ != NULL) {
		CancelWaitableTimer(windows_timer_);
		is_timer_running_ = false;
	}
}

bool WindowsTimer::waitForTimer(DWORD timeout) {
	if (windows_timer_ == NULL) {
		return false;
	}
	DWORD result = WaitForSingleObject(windows_timer_, timeout);
	return result == WAIT_OBJECT_0;
}

void WindowsTimer::close() {
	if (windows_timer_ != NULL) {
		CloseHandle(windows_timer_);
		windows_timer_ = NULL;
		is_timer_running_ = false;
	}
}
