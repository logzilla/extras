/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once

#include <string>
#include <vector>

namespace Syslog_agent {

    class Registry {
    public:
        Registry();
        ~Registry();
        void close();
        void open();
        void open(HKEY parent, const wchar_t* name);;
        void open(Registry& parent, const wchar_t* name);;
        bool readBool(const wchar_t* name, bool default_value) const;
        char readChar(const wchar_t* name, char default_value) const;
        int readInt(const wchar_t* name, int default_value) const;
        time_t readTime(const wchar_t* name, time_t default_value) const;
        std::wstring readString(const wchar_t* name, wchar_t* default_value) const;
        void writeUint(const wchar_t* name, DWORD value) const;
        void writeTime(const wchar_t* name, time_t value) const;
        std::vector<std::wstring> readChannels() const;
        static std::wstring readBookmark(const wchar_t* channel);
        static void writeBookmark(const wchar_t* channel, const wchar_t* bookmark);
        static void loadSetupFile();
    private:
        std::wstring readSubkey(HKEY registry_key, DWORD index) const;
        HKEY main_key_;
        HKEY channels_key_;
        bool is_open_;
    };
}
