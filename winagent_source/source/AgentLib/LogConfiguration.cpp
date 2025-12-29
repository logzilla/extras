/*
SyslogAgent: a syslog agent for Windows
Copyright 2021 Logzilla Corp.
*/


#include "pch.h"
// #include "stdafx.h" // Removed, pch.h is used
#include "LogConfiguration.h"
#include "SyslogAgentSharedConstants.h" // Added include
#include <debugapi.h> // Added for OutputDebugStringW

using namespace Syslog_agent;

void LogConfiguration::loadFromRegistry(Registry& parent) {
    OutputDebugStringW((L"DEBUG: LogConfiguration::loadFromRegistry - Entering for channel: " + channel_ + L"\n").c_str());
    // Read per-channel enabled flag and bookmark from the channel subkey.
    enabled_ = Registry::readChannelEnabled(channel_.c_str());
    OutputDebugStringW((L"DEBUG: LogConfiguration::loadFromRegistry - Enabled for: " + channel_ + L" = " + (enabled_ ? L"true" : L"false") + L"\n").c_str());
    OutputDebugStringW((L"DEBUG: LogConfiguration::loadFromRegistry - Reading bookmark for: " + channel_ + L"\n").c_str());
    bookmark_ = Registry::readBookmark(channel_.c_str());
    OutputDebugStringW((L"DEBUG: LogConfiguration::loadFromRegistry - Read bookmark for: " + channel_ + L", value: " + bookmark_ + L"\n").c_str());
    OutputDebugStringW((L"DEBUG: LogConfiguration::loadFromRegistry - Exiting for channel: " + channel_ + L"\n").c_str());
}

void LogConfiguration::saveToRegistry(Registry& parent) const {
    return;
    const wchar_t* bookmark_str = bookmark_.c_str();
    Registry::writeBookmark(channel_.c_str(), bookmark_str, static_cast<DWORD>(wcslen(bookmark_str) * sizeof(wchar_t)));
}
