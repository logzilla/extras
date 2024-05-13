/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#include "stdafx.h"

#include <iostream>
#include <fstream>

#include "Logger.h"
#include "Registry.h"
#include "Result.h"
#include "SyslogAgentSharedConstants.h"
#include "Util.h"

using namespace Syslog_agent;

Registry::Registry() : channels_key_(nullptr) {
    is_open_ = false;
    main_key_ = nullptr;
}


void Registry::close() {
    if (!is_open_)
        return;
    RegCloseKey(main_key_);
    RegCloseKey(channels_key_);
    is_open_ = false;
}


void Registry::open() {
    auto status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
        SharedConstants::RegistryKey::MAIN_KEY, 0, KEY_READ, &main_key_);
    if (status != ERROR_SUCCESS) {
        throw Result(status, "Registry::open()", "RegOpenKeyEx for main key");
    }
    else {
        status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, SharedConstants::RegistryKey::CHANNELS_KEY, 
            0, KEY_READ | KEY_ENUMERATE_SUB_KEYS, &channels_key_);
        if (status != ERROR_SUCCESS) {
            channels_key_ = nullptr;
        }
    is_open_ = true;
    }
}


void Registry::open(HKEY parent, const wchar_t* name) {
    auto status = RegOpenKeyEx(parent, name, 0, KEY_READ | KEY_WRITE, &main_key_);
    if (status == ERROR_SUCCESS) {
        is_open_ = true;
        return;
    }
    throw Result(status, "Registry::open()", "RegOpenKeyEx");
}


void Registry::open(Registry& parent, const wchar_t* name) {
    open(parent.main_key_, name);
}


Registry::~Registry() {
    close();
}


bool Registry::readBool(const wchar_t* name, bool default_value) const {
    bool value;
    DWORD size = sizeof value;
    auto status = RegQueryValueEx(main_key_, name, nullptr, nullptr, (LPBYTE)&value, &size);
    if (status == ERROR_SUCCESS) return value;
    if (status == ERROR_FILE_NOT_FOUND) return default_value;
    throw Result(status, "Registry::readBool()", "RegQueryValueEx");
}


char Registry::readChar(const wchar_t* name, char default_value) const {
    char value;
    DWORD size = sizeof value;
    auto status = RegQueryValueEx(main_key_, name, nullptr, nullptr, (LPBYTE)&value, &size);
    if (status == ERROR_SUCCESS) return value;
    if (status == ERROR_FILE_NOT_FOUND) return default_value;
    throw Result(status, "Registry::readChar()", "RegQueryValueEx");
}


int Registry::readInt(const wchar_t* name, int default_value) const {
    DWORD value;
    DWORD size = sizeof value;
    auto status = RegQueryValueEx(main_key_, name, nullptr, nullptr, (LPBYTE)&value, &size);
    if (status == ERROR_SUCCESS) return value;
    if (status == ERROR_FILE_NOT_FOUND) return default_value;
    throw Result(status, "Registry::readInt()", "RegQueryValueEx");
}


std::wstring Registry::readString(const wchar_t* name, wchar_t* default_value) const {
    wchar_t value[1024];
    DWORD size = sizeof value;
    auto status = RegQueryValueEx(main_key_, name, nullptr, nullptr, (LPBYTE)&value, &size);
    if (status == ERROR_SUCCESS) return std::wstring(value);
    if (status == ERROR_FILE_NOT_FOUND) return std::wstring(default_value);
    throw Result(status, "Registry::readString()", "RegQueryValueEx");
}


time_t Registry::readTime(const wchar_t* name, time_t default_value) const {
    time_t value;
    DWORD size = sizeof value;
    auto status = RegQueryValueEx(main_key_, name, nullptr, nullptr, (LPBYTE)&value, &size);
    if (status == ERROR_SUCCESS) return value;
    if (status == ERROR_FILE_NOT_FOUND) return default_value;
    throw Result(status, "Registry::readTime()", "RegQueryValueEx");
}


std::wstring Registry::readSubkey(HKEY registry_key, DWORD index) const {
    wchar_t value[1024];
    DWORD size = sizeof value;
    auto status = RegEnumKeyEx(registry_key, index, (LPWSTR)&value, &size, nullptr, 
        nullptr, nullptr, nullptr);
    if (status == ERROR_SUCCESS) return std::wstring(value);
    if (status == ERROR_NO_MORE_ITEMS) return std::wstring();
    throw Result(status, "Registry::readSubkey()", "RegEnumKeyEx");
}


void Registry::writeUint(const wchar_t* name, DWORD value) const {
    DWORD size = sizeof value;
    auto status = RegSetValueEx(main_key_, name, 0, REG_DWORD, (LPBYTE)&value, size);
    if (status == ERROR_SUCCESS) return;
    throw Result(status, "Registry::writeUint()", "RegSetValueEx");
}


void Registry::writeTime(const wchar_t* name, time_t value) const {
    DWORD size = sizeof value;
    auto status = RegSetValueEx(main_key_, name, 0, REG_QWORD, (LPBYTE)&value, size);
    if (status == ERROR_SUCCESS) return;
    throw Result(status, "Registry::writeTime()", "RegSetValueEx");
}


std::vector<std::wstring> Registry::readChannels() const {
    std::vector<std::wstring> channels;
    if (channels_key_ == nullptr) {
        return channels;
    }
    wchar_t channel_name[1024];
    DWORD status = ERROR_SUCCESS;
    for (DWORD i = 0; true; i++) {
        DWORD channel_name_size = sizeof channel_name;
        status = RegEnumKeyEx(channels_key_, i, (LPWSTR)&channel_name, &channel_name_size, 
            nullptr, nullptr, nullptr, nullptr);
        if (status == ERROR_NO_MORE_ITEMS)
            break;
        if (status != 0) {
            throw Result(status, "Registry::readSubkey()", "RegEnumKeyEx");
        }
        HKEY channel_key = NULL;
        wchar_t full_channel_path[4096];
        swprintf_s(full_channel_path, 4096, L"%s\\%s", 
            SharedConstants::RegistryKey::CHANNELS_KEY, channel_name);
        auto status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, full_channel_path, 0, 
            KEY_READ, &channel_key);
        if (status != ERROR_SUCCESS) {
            throw Result(status, "Registry::readChannels()", "could not open channel");
        }
        DWORD value;
        DWORD value_size = sizeof value;
        status = RegQueryValueEx(channel_key, SharedConstants::RegistryKey::CHANNEL_ENABLED, 
            nullptr, nullptr, (LPBYTE)&value, &value_size);
        RegCloseKey(channel_key);
        if (status != ERROR_SUCCESS) {
            throw Result(status, "Registry::readChannels()", "could not read channel");
        }
        if (value == 1) {
            channels.push_back(channel_name);
        }
    }
    return channels;
}


std::wstring Registry::readBookmark(const wchar_t* channel) {
    HKEY channel_key;
    DWORD status = ERROR_SUCCESS;
    wchar_t tempbuf[4096];
    swprintf_s(tempbuf, 4096, L"%s\\%s",
        SharedConstants::RegistryKey::CHANNELS_KEY, channel);
    status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, tempbuf, 0,
        KEY_READ, &channel_key);
    if (status != ERROR_SUCCESS) {
        DWORD error = GetLastError();
        // save stack space, use same buf
        Util::toPrintableAscii(reinterpret_cast<char*>(tempbuf), sizeof(tempbuf), channel, ' ');
        Logger::recoverable_error("Registry::readBookmark()> error %d,"
            " could not open channel %s\n", error, reinterpret_cast<char*>(tempbuf));
        return wstring();
    }
    DWORD xml_size = sizeof tempbuf;
    status = RegQueryValueEx(channel_key, 
        SharedConstants::RegistryKey::CHANNEL_BOOKMARK, nullptr, nullptr, 
        (LPBYTE)&tempbuf, &xml_size);
    if (status != ERROR_SUCCESS) {
        DWORD error = GetLastError();
        char warnbuf[1024];
        Util::toPrintableAscii(warnbuf, sizeof(warnbuf), channel, ' ');
        Logger::warn("Registry::readBookmark()> error %d, could not read"
            " bookmark for %s\n", error, warnbuf);
        tempbuf[0] = 0;
        // fall through to close channel
    }
    RegCloseKey(channel_key);
    return wstring(tempbuf);
}


void Registry::writeBookmark(const wchar_t* channel, const wchar_t* bookmark) {
    HKEY channel_key;
    DWORD status = ERROR_SUCCESS;
    wchar_t full_channel_path[4096];
    swprintf_s(full_channel_path, 4096, L"%s\\%s", 
        SharedConstants::RegistryKey::CHANNELS_KEY, channel);
    status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, full_channel_path, 0, 
        KEY_READ | KEY_SET_VALUE, &channel_key);
    if (status != ERROR_SUCCESS) {
        throw Result(status, "Registry::writeBookmark()", "could not open channel");
    }
    status = RegSetValueEx(channel_key, SharedConstants::RegistryKey::CHANNEL_BOOKMARK, 
        0, REG_SZ, (LPBYTE)bookmark, static_cast<DWORD>(wcslen(bookmark)) * sizeof(wchar_t));
    if (status != ERROR_SUCCESS) {
        throw Result(status, "Registry::writeBookmark()", "could not write bookmark");
    }
    RegCloseKey(channel_key);
}


void Registry::loadSetupFile() {
    HKEY main_key;
    LSTATUS status;
    DWORD size;

    status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, SharedConstants::RegistryKey::MAIN_KEY, 
        0, KEY_READ | KEY_WRITE, &main_key);
    if (status != ERROR_SUCCESS) {
        // can't open registry key, just return
        return;
    }

    wchar_t setup_filename[1024];
    size = sizeof setup_filename;
    status = RegQueryValueEx(main_key, SharedConstants::RegistryKey::INITIAL_SETUP_FILE,
        nullptr, nullptr, (LPBYTE)&setup_filename, &size);
    RegCloseKey(main_key);
    if (status != ERROR_SUCCESS) {
        // can't read setup filename, just return
        return;
    }


    // Acquiring required privileges	
    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
    {
        PTOKEN_PRIVILEGES ns = (PTOKEN_PRIVILEGES)new BYTE[sizeof(DWORD) 
            + sizeof(LUID_AND_ATTRIBUTES) + 2];
        if (LookupPrivilegeValue(NULL, SE_BACKUP_NAME, &(ns->Privileges[0].Luid)))
        {
            ns->PrivilegeCount = 1;
            ns->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            if (!AdjustTokenPrivileges(hToken, FALSE, ns, 0, NULL, NULL))
            {
				Logger::fatal("Registry::loadSetupFile()> error %d, could not enable"
					" SE_BACKUP_NAME privilege\n", GetLastError());
            }
        }
        if (LookupPrivilegeValue(NULL, SE_RESTORE_NAME, &(ns->Privileges[0].Luid)))
        {
            ns->PrivilegeCount = 1;
            ns->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            if (!AdjustTokenPrivileges(hToken, FALSE, ns, 0, NULL, NULL))
            {
				Logger::fatal("Registry::loadSetupFile()> error %d, could not enable"
					" SE_RESTORE_NAME privilege\n", GetLastError());
            }

        }
        delete[](BYTE*)ns;
        CloseHandle(hToken);
    }

    status = RegRestoreKey(HKEY_LOCAL_MACHINE, setup_filename, 0);


    // load cert files
    wchar_t cert_filename[1024];
    size = sizeof cert_filename;
    status = RegQueryValueEx(main_key, SharedConstants::RegistryKey::PRIMARY_TLS_FILENAME, 
        nullptr, nullptr, (LPBYTE)&cert_filename, &size);
    if (status == ERROR_SUCCESS) {
        wstring primary_cert_path = Util::getThisPath(true) + SharedConstants::CERT_FILE_PRIMARY;
        Util::copyFile(cert_filename, primary_cert_path.c_str()); // if this fails, just ignore
    }

    status = RegQueryValueEx(main_key, SharedConstants::RegistryKey::SECONDARY_TLS_FILENAME, 
        nullptr, nullptr, (LPBYTE)&cert_filename, &size);
    if (status == ERROR_SUCCESS) {
        wstring secondary_cert_path = Util::getThisPath(true) 
            + SharedConstants::CERT_FILE_SECONDARY;
        Util::copyFile(cert_filename, secondary_cert_path.c_str()); // if this fails, just ignore
    }

    RegCloseKey(main_key);

    // delete setup regkey
    wchar_t keyname_buf[1024];
    swprintf_s(keyname_buf, _countof(keyname_buf), L"%s\\%s", 
        SharedConstants::RegistryKey::MAIN_KEY, SharedConstants::RegistryKey::INITIAL_SETUP_FILE);
    status = RegDeleteKey(HKEY_LOCAL_MACHINE, keyname_buf);
}
