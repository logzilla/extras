#pragma once
#include <Windows.h>

class WindowsTimer {
public:
	WindowsTimer();
	~WindowsTimer();
	void startTimer(DWORD milliseconds);
	void stopTimer();
	bool waitForTimer(DWORD timeout);
	void close();

private:
	HANDLE windows_timer_;
	bool is_timer_running_;
};
