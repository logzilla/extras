#pragma once
/*
SyslogAgent: a syslog agent for Windows
Copyright 2021 Logzilla Corp.
*/

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cctype>
#include <string>
#include <vector>
#include <wchar.h>

#ifdef INFRASTRUCTURE_EXPORTS
#define INFRA_API __declspec(dllexport)
#else
#define INFRA_API __declspec(dllimport)
#endif

class INFRA_API Util {
public:
    static void toPrintableAscii(char* destination, int destination_count,
        const wchar_t* source, char space_replacement);
    static size_t toLowercase(wchar_t* str);
    static size_t toLowercase(char* str);
    static std::wstring toLowercase(const std::wstring& str);
    // New method that uses buffer instead of wstring
    static bool getThisPath(wchar_t* buffer, size_t buffer_size, bool with_trailing_backslash = true);
    static std::string readFileAsString(const char* filename);
    static std::string readFileAsString(const wchar_t* filename);
    static void replaceAll(std::string& str, const std::string& from, const std::string& to);
    static size_t hashWstring(const std::wstring& _Keyval);
    static size_t wstr2str(char* dest, size_t dest_size, const wchar_t* src);
    static size_t wstr2str_truncate(char* dest, size_t dest_size, const wchar_t* src);
    static int jsonEscape(char* input_buffer, char* output_buffer, int output_buffer_length);
    static size_t jsonEscapeString(const char* input, char* output_buffer, size_t output_buffer_size);
    static bool copyFile(const wchar_t* const source_filename, const wchar_t* const dest_filename);
    static int64_t getUnixTimeMilliseconds();
    static void epochToDateTime(const char* epochStr, char* output);
    static int compareSoftwareVersions(const char* a, const char* b);
    static std::vector<int> splitVersion(const std::string& version);
    static std::string getTempDirectory();
    static char getPathSeparator();
    static std::string getAppropriateLogPath(const std::string& logFileName);
    
    struct UrlComponents {
        // Fixed-size character arrays instead of std::wstring for safe cross-module boundaries
        wchar_t hostName[256];
        wchar_t path[1024];
        unsigned int port{ 0 };
        bool isSecure{ false };
        bool hasExplicitPort{ false };  // true if port was explicitly specified in URL
    };

    static bool ParseUrl(const wchar_t* url, UrlComponents& components);

    // ------- TEST HOOKS -------
    // Typedef for a function matching the signature of GetModuleFileNameW so
    // that unit tests can supply an alternative implementation without
    // resorting to linker tricks or detours.
    using ModulePathProviderFunc = DWORD (WINAPI *)(HMODULE, LPWSTR, DWORD);

    // Set a custom provider.  Pass nullptr to restore the default
    // ::GetModuleFileNameW.  This is intended for **unit-testing only** and is
    // not used in production code paths.
    static void setModulePathProvider(ModulePathProviderFunc func);
};
