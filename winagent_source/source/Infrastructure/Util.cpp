/*
SyslogAgent: a syslog agent for Windows
Copyright 2021 Logzilla Corp.
*/

#include "pch.h"
#include <algorithm>
#include <cctype>
#include <clocale>
#include <codecvt>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <string>
#include <tlhelp32.h>
#include <vector>
#include <Psapi.h>
#include "Util.h"
#include "Logger.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <winhttp.h>
#else
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif



using namespace std;

// ---------------------------------------------------------------------------
// Optional dependency-injection hook so unit tests can provide a custom
// implementation for obtaining the executable module path.  In production this
// pointer remains nullptr and the real ::GetModuleFileNameW is used.
// ---------------------------------------------------------------------------
namespace {
    // global within this translation unit; default is nullptr (use real API)
    Util::ModulePathProviderFunc g_customModulePathProvider = nullptr;
}

void Util::setModulePathProvider(ModulePathProviderFunc func)
{
    g_customModulePathProvider = func;
}

void Util::toPrintableAscii(char* destination, int destination_count,
    const wchar_t* source, char space_replacement) {
    auto logger = LOG_THIS;

    int i;
    for (i = 0; i < destination_count - 1; i++) {
        if (source[i] == 0) break;
        if (source[i] < 32 || source[i] > 126) destination[i] = '?';
        else if (source[i] == 32) destination[i] = space_replacement;
        else destination[i] = static_cast<char>(source[i]);
    }
    destination[i] = 0;
}

/*
 * DEPRECATED: Use wstr2str_truncate instead for more predictable behavior and explicit
 * truncation handling. This function is retained only for backward compatibility.
 */
size_t Util::wstr2str(char* dest, size_t dest_size, const wchar_t* src)
{
    // Input validation with early return
    if (!dest || !src || dest_size == 0) return 0;

    // Ensure buffer is always null-terminated even if conversion fails
    dest[0] = '\0';
    dest[dest_size - 1] = '\0';

    // Get required buffer size first to properly handle cases where dest_size is too small
    int required_size = WideCharToMultiByte(CP_UTF8, 0, src, -1, NULL, 0, NULL, NULL);
    if (required_size <= 0) {
        // Conversion error (invalid UTF-16 sequence or other Windows API error)
        return 0;
    }

    // Perform the actual conversion
    size_t converted = 0;
    int result = WideCharToMultiByte(CP_UTF8, 0, src, -1, dest, static_cast<int>(dest_size), NULL, NULL);
    if (result > 0) {
        // Success - get actual string length (excluding null terminator)
        converted = strlen(dest);
    }

    // Double-check null termination as a safety measure
    dest[dest_size - 1] = '\0';
    return converted;
}

size_t Util::wstr2str_truncate(char* dest, size_t dest_size, const wchar_t* src)
{
    // Input validation with early return
    if (!dest || !src || dest_size == 0) return 0;

    // Ensure buffer starts empty and is always null-terminated
    dest[0] = '\0';
    dest[dest_size - 1] = '\0';

    // Get required buffer size to check if truncation will occur
    int required_size = WideCharToMultiByte(CP_UTF8, 0, src, -1, NULL, 0, NULL, NULL);
    if (required_size <= 0) {
        // Conversion error (invalid UTF-16 sequence or other Windows API error)
        return 0;
    }

    // Perform the actual conversion
    int result = WideCharToMultiByte(CP_UTF8, 0, src, -1, dest, static_cast<int>(dest_size), NULL, NULL);
    
    // Handle success and truncation cases
    if (result > 0) {
        // Success case
        size_t actual_length = strlen(dest);
        if (static_cast<int>(actual_length) + 1 < required_size) {
            // Truncation occurred
            return actual_length;
        }
        return actual_length;
    }
    
    // If WideCharToMultiByte failed (result == 0), ensure string is still properly terminated
    dest[0] = '\0';
    dest[dest_size - 1] = '\0';
    return 0;
}

size_t Util::toLowercase(wchar_t* str) {
    if (!str) return 0;

    size_t count = 0;
    while (str[count]) {
        str[count] = towlower(str[count]);
        ++count;
    }
    return count;
}

size_t Util::toLowercase(char* str) {
    size_t i;
    for (i = 0; str[i] != 0; i++) {
        str[i] = tolower(str[i]);
    }
    return i;
}

std::wstring Util::toLowercase(const std::wstring& str) {
    std::wstring result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::towlower);
    return result;
}

bool Util::getThisPath(wchar_t* buffer, size_t buffer_size, bool with_trailing_backslash) {
    // Validate input parameters
    if (!buffer || buffer_size < MAX_PATH) return false;
    
    // Canary to detect overwrite in this function
    volatile uint32_t local_canary = 0xDEADBEEF;
    
    // Zero out buffer first *within the function* for extra safety
    // Even if the caller initialized it, this ensures it's zeroed here.
    memset(buffer, 0, buffer_size * sizeof(wchar_t));
    
    // Get full module path into the buffer - use buffer_size-1 to ensure space for null terminator
    DWORD result = 0;
    if (g_customModulePathProvider) {
        result = g_customModulePathProvider(NULL, buffer, static_cast<DWORD>(buffer_size-1));
    }
    else {
        result = GetModuleFileNameW(NULL, buffer, static_cast<DWORD>(buffer_size-1));
    }
    
    // Check for overwrite
    if (local_canary != 0xDEADBEEF) {
        __debugbreak();
    }
    
    // Check for errors or truncation
    if (result == 0) return false; // GetModuleFileNameW failed
    if (result >= buffer_size-1) {
        buffer[buffer_size-1] = L'\0'; // Ensure null termination
        return false; // Path was truncated
    }
    
    // Ensure null termination (shouldn't be needed but added for safety)
    buffer[result] = L'\0';
    
    // Find last backslash
    wchar_t* last_slash = wcsrchr(buffer, L'\\');
    if (!last_slash) return false;
    
    // Null terminate after the last slash to get the directory using index-based access
    size_t last_slash_index = last_slash - buffer;
    // Ensure index is valid and within bounds (though it should be)
    if (last_slash_index + 1 < buffer_size) {
        buffer[last_slash_index + 1] = L'\0';
    } else {
        // This case should ideally not happen given previous checks, but handle defensively
        return false; 
    }
    
    // Check if there's already a trailing backslash
    size_t path_len = wcslen(buffer); // wcslen is safe now due to null termination
    bool has_trailing_slash = (path_len > 0 && buffer[path_len-1] == L'\\');
    
    // Add trailing backslash if requested and not already present
    if (with_trailing_backslash && !has_trailing_slash) {
        // Check if we have enough space for the trailing slash
        if (path_len + 2 > buffer_size) return false; // +2 for \ and null terminator
        
        // Add trailing backslash
        buffer[path_len] = L'\\';
        buffer[path_len + 1] = L'\0';
    } else if (!with_trailing_backslash && has_trailing_slash) {
        // Remove trailing backslash if present but not wanted
        buffer[path_len-1] = L'\0';
    }
    
    return true;
}

string Util::readFileAsString(const char* filename) {
    ifstream infile(filename);
    if (!infile) {
        return string();
    }
    stringstream buffer;
    buffer << infile.rdbuf();
    return buffer.str();
}

string Util::readFileAsString(const wchar_t* filename) {
    FILE* infile;
    _wfopen_s(&infile, filename, L"r");
    if (!infile) {
        return string();
    }

    fseek(infile, 0, SEEK_END);
    int64_t fsize = ftell(infile);
    fseek(infile, 0, SEEK_SET);
    vector<char> contents(fsize + 1);
    fread(contents.data(), 1, fsize, infile);
    fclose(infile);
    contents[fsize] = 0;
    return string(contents.data(), fsize);
}

void Util::replaceAll(std::string& str, const std::string& from,
    const std::string& to) {
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
        // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

size_t Util::hashWstring(const std::wstring& _Keyval)
{    // hash _Keyval to size_t value by pseudorandomizing transform
    size_t _Val = 2166136261U;
    size_t _First = 0;
    size_t _Last = _Keyval.size();
    size_t _Stride = 1 + _Last / 10;

    if (_Stride < _Last)
        _Last -= _Stride;
    for (; _First < _Last; _First += _Stride)
        _Val = 16777619U * _Val ^ (size_t)_Keyval[_First];
    return (_Val);
}

int Util::jsonEscape(char* input_buffer, char* output_buffer,
    int output_buffer_length) {
    int output_pos = 0;
    for (int i = 0;
        output_pos < output_buffer_length - 1 && input_buffer[i] != 0;
        ++i) {
        unsigned char cur_char = static_cast<unsigned char>(input_buffer[i]);

        // Handle control characters (0x00-0x1F)
        if (cur_char < 0x20) {
            if (output_pos + 6 >= output_buffer_length - 1) break;  // Need room for \u00XX

            // Special handling for common control chars
            switch (cur_char) {
            case '\b': // backspace
                output_buffer[output_pos++] = '\\';
                output_buffer[output_pos++] = 'b';
                break;
            case '\f': // form feed
                output_buffer[output_pos++] = '\\';
                output_buffer[output_pos++] = 'f';
                break;
            case '\n': // newline
                output_buffer[output_pos++] = '\\';
                output_buffer[output_pos++] = 'n';
                break;
            case '\r': // carriage return
                output_buffer[output_pos++] = '\\';
                output_buffer[output_pos++] = 'r';
                break;
            case '\t': // tab
                output_buffer[output_pos++] = '\\';
                output_buffer[output_pos++] = 't';
                break;
            default:
                // Use \u00XX format for other control chars
                output_buffer[output_pos++] = '\\';
                output_buffer[output_pos++] = 'u';
                output_buffer[output_pos++] = '0';
                output_buffer[output_pos++] = '0';
                output_buffer[output_pos++] = "0123456789ABCDEF"[(cur_char >> 4) & 0x0F];
                output_buffer[output_pos++] = "0123456789ABCDEF"[cur_char & 0x0F];
                break;
            }
        }
        else if (cur_char == '"' || cur_char == '\\') {
            // Quote and backslash need escaping
            if (output_pos + 2 >= output_buffer_length - 1) break;
            output_buffer[output_pos++] = '\\';
            output_buffer[output_pos++] = cur_char;
        }
        else if (cur_char >= 0x20 && cur_char <= 0x7F) {
            // Printable ASCII
            if (output_pos + 1 >= output_buffer_length - 1) break;
            output_buffer[output_pos++] = cur_char;
        }
        else {
            // Non-ASCII characters get \u escaping
            if (output_pos + 6 >= output_buffer_length - 1) break;
            output_buffer[output_pos++] = '\\';
            output_buffer[output_pos++] = 'u';
            output_buffer[output_pos++] = '0';
            output_buffer[output_pos++] = '0';
            output_buffer[output_pos++] = "0123456789ABCDEF"[(cur_char >> 4) & 0x0F];
            output_buffer[output_pos++] = "0123456789ABCDEF"[cur_char & 0x0F];
        }
    }
    output_buffer[output_pos] = 0;
    return output_pos + 1;
}

size_t Util::jsonEscapeString(const char* input, char* output_buffer, size_t output_buffer_size) {
    if (!input || !output_buffer || output_buffer_size == 0) {
        if (output_buffer && output_buffer_size > 0) output_buffer[0] = '\0';
        return 0;
    }

    size_t input_len = strlen(input);
    if (input_len == 0) {
        output_buffer[0] = '\0';
        return 0;
    }

    // Call jsonEscape which already handles the escaping logic
    int result = jsonEscape(const_cast<char*>(input), output_buffer, static_cast<int>(output_buffer_size));
    return result > 0 ? static_cast<size_t>(result - 1) : 0;  // -1 to not count null terminator
}

bool Util::copyFile(const wchar_t* const source_filename, const wchar_t* const dest_filename)
{
    ifstream src(source_filename, ios::binary);
    if (!src) {
        return false;
    }

    ofstream dest(dest_filename, ios::binary);
    if (!dest) {
        return false;
    }

    dest << src.rdbuf();

    src.close();
    dest.close();

    return true;
}

#if MAYBE_THIS_WILL_BE_NEEDED

static void EnumerateOpenFileHandles(DWORD processId)
{
    HANDLE hFileSnap = CreateToolhelp32Snapshot(TH32CS_SNAPALL, processId);
    if (hFileSnap == INVALID_HANDLE_VALUE)
    {
        printf("Error: CreateToolhelp32Snapshot failed.\n");
        return;
    }

    printf("Open file handles for process %d:\n", processId);

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (!Process32First(hFileSnap, &pe32))
    {
        printf("Error: Process32First failed.\n");
        CloseHandle(hFileSnap);
        return;
    }

    do
    {
        if (pe32.th32ProcessID != processId)
            continue;

        HANDLE hFile = CreateFile(pe32.szExeFile, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            printf("%s\n", pe32.szExeFile);
            CloseHandle(hFile);
        }
    } while (Process32Next(hFileSnap, &pe32));

    CloseHandle(hFileSnap);
}

/* to use this function you must include winsock2.h and iphlpapi.h, and link in iphlpapi.lib */
#include <winsock2.h>
#include <iphlpapi.h>
#include <stdio.h>

static void EnumerateOpenTcpSockets(DWORD processId)
{
    DWORD dwSize = 0;
    ULONG ulRetVal = 0;

    // Retrieve the TCP table.
    PMIB_TCPTABLE2 pTcpTable = NULL;
    if (GetTcpTable2(NULL, &dwSize, TRUE) == ERROR_INSUFFICIENT_BUFFER)
    {
        pTcpTable = (PMIB_TCPTABLE2)malloc(dwSize);
        if (pTcpTable == NULL)
        {
            printf("Error: Memory allocation failed.\n");
            return;
        }
    }

    if ((ulRetVal = GetTcpTable2(pTcpTable, &dwSize, TRUE)) != NO_ERROR)
    {
        printf("Error: GetTcpTable2 failed with error %lu.\n", ulRetVal);
        free(pTcpTable);
        return;
    }

    // Enumerate the TCP connections and filter by process ID.
    for (DWORD i = 0; i < pTcpTable->dwNumEntries; i++)
    {
        PMIB_TCPROW2 pTcpRow = &pTcpTable->table[i];
        if (pTcpRow->dwOwningPid == processId)
        {
            printf("TCP connection %d.%d.%d.%d:%d -> %d.%d.%d.%d:%d\n",
                (pTcpRow->dwLocalAddr >> 0) & 0xff,
                (pTcpRow->dwLocalAddr >> 8) & 0xff,
                (pTcpRow->dwLocalAddr >> 16) & 0xff,
                (pTcpRow->dwLocalAddr >> 24) & 0xff,
                ntohs((unsigned short)pTcpRow->dwLocalPort),
                (pTcpRow->dwRemoteAddr >> 0) & 0xff,
                (pTcpRow->dwRemoteAddr >> 8) & 0xff,
                (pTcpRow->dwRemoteAddr >> 16) & 0xff,
                (pTcpRow->dwRemoteAddr >> 24) & 0xff,
                ntohs((unsigned short)pTcpRow->dwRemotePort));
        }
    }

    free(pTcpTable);
}
#endif

int64_t Util::getUnixTimeMilliseconds() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);  // Retrieves the current system time in UTC

    // Combine high and low parts to form a 64-bit value
    ULARGE_INTEGER li{ 0 };
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;

    // Convert FILETIME (100-nanoseconds since January 1, 1601) to 
    // Unix epoch time in milliseconds
    int64_t unixTimeMilliseconds = (li.QuadPart - 116444736000000000LL) / 10000;

    return unixTimeMilliseconds;
}

void Util::epochToDateTime(const char* epochStr, char* output) {
    std::time_t epoch = std::atoll(epochStr);
    std::tm timeinfo{}; // Initialized variable
    localtime_s(&timeinfo, &epoch);
    std::strftime(output, 20, "%Y-%m-%d %H:%M:%S", &timeinfo);
}


int Util::compareSoftwareVersions(const char* a, const char* b) {
    while (*a || *b) {
        // Parse next numeric token from a
        int valA = 0;
        while (*a && *a != '.' && *a != '-') {
            if (isdigit(*a)) {
                valA = valA * 10 + (*a - '0');
                ++a;
            }
            else {
                break;
            }
        }

        // Parse next numeric token from b
        int valB = 0;
        while (*b && *b != '.' && *b != '-') {
            if (isdigit(*b)) {
                valB = valB * 10 + (*b - '0');
                ++b;
            }
            else {
                break;
            }
        }

        if (valA < valB) return -1;
        if (valA > valB) return 1;

        // Skip dots if any
        if (*a == '.') ++a;
        if (*b == '.') ++b;

        // Handle pre-release indicators (e.g. -rc1)
        if ((*a == '-' || *b == '-') && *a != *b) {
            if (*a == '-') return -1; // a is pre-release, b is stable
            if (*b == '-') return 1;  // b is pre-release, a is stable
        }

        // If both are pre-release parts, compare lexicographically
        if (*a == '-' && *b == '-') {
            ++a;
            ++b;
            const char* subA = a;
            const char* subB = b;
            while (*a && *a != '.' && *a != '-') ++a;
            while (*b && *b != '.' && *b != '-') ++b;

            int subLenA = a - subA;
            int subLenB = b - subB;

            int cmp = strncmp(subA, subB, (std::min)(subLenA, subLenB));
            if (cmp != 0) return cmp < 0 ? -1 : 1;

            if (subLenA < subLenB) return -1;
            if (subLenA > subLenB) return 1;
        }
    }

    return 0; // Equal
}

std::vector<int> Util::splitVersion(const std::string& version) {
    std::vector<int> parts;
    std::string clean_version;

    // Find the first occurrence of a non-numeric separator (-, ~, etc)
    size_t suffix_pos = version.find_first_of("-~+");
    if (suffix_pos != std::string::npos) {
        clean_version = version.substr(0, suffix_pos);
    }
    else {
        clean_version = version;
    }

    std::istringstream ss(clean_version);
    std::string token;

    while (std::getline(ss, token, '.')) {
        if (!token.empty()) {
            // Find the first numeric part in the token
            size_t start = token.find_first_of("0123456789");
            if (start != std::string::npos) {
                size_t end = token.find_first_not_of("0123456789", start);
                std::string numericPart = token.substr(start, end - start);
                try {
                    parts.push_back(std::stoi(numericPart));
                }
                catch (const std::invalid_argument&) {
                    parts.push_back(0); // Default to zero if conversion fails
                }
            }
            else {
                // If no numeric part found, treat it as zero
                parts.push_back(0);
            }
        }
        else {
            // If token is empty, treat it as zero
            parts.push_back(0);
        }
    }

    return parts;
}

bool Util::ParseUrl(const wchar_t* url, UrlComponents& components) {
    auto logger = LOG_THIS;

    if (!url || wcslen(url) == 0) {
        logger->recoverable_error("ParseUrl() empty URL\n");
        return false;
    }

    std::wstring urlStr(url);
    
    // Parse scheme (http/https)
    size_t schemeEnd = urlStr.find(L"://");
    if (schemeEnd == std::wstring::npos) {
        // No scheme specified, assume http
        components.isSecure = false;
    } else {
        std::wstring scheme = urlStr.substr(0, schemeEnd);
        // Convert to lowercase for comparison
        std::transform(scheme.begin(), scheme.end(), scheme.begin(), ::towlower);
        components.isSecure = (scheme == L"https");
        urlStr = urlStr.substr(schemeEnd + 3); // Skip past "://"
    }

    // Find end of host (marked by '/' or ':')
    size_t hostEnd = urlStr.find_first_of(L":/");
    if (hostEnd == std::wstring::npos) {
        hostEnd = urlStr.length();
    }
    
    // Copy hostname to fixed buffer with bounds checking
    if (hostEnd >= sizeof(components.hostName) / sizeof(wchar_t)) {
        logger->recoverable_error("ParseUrl() hostname too long\n");
        return false;
    }
    wcsncpy_s(components.hostName, sizeof(components.hostName) / sizeof(wchar_t), 
              urlStr.substr(0, hostEnd).c_str(), _TRUNCATE);
    
    if (components.hostName[0] == '\0') {
        logger->recoverable_error("ParseUrl() no hostname found\n");
        return false;
    }

    // Parse port if present
    size_t portStart = urlStr.find(L':', hostEnd);
    if (portStart != std::wstring::npos) {
        size_t pathStart = urlStr.find(L'/', portStart);
        std::wstring portStr;
        if (pathStart != std::wstring::npos) {
            portStr = urlStr.substr(portStart + 1, pathStart - portStart - 1);
        } else {
            portStr = urlStr.substr(portStart + 1);
        }
        
        try {
            components.port = std::stoi(portStr);
            components.hasExplicitPort = true;
        } catch (const std::exception&) {
            logger->recoverable_error("ParseUrl() invalid port number\n");
            return false;
        }
        
        if (components.port <= 0 || components.port > 65535) {
            logger->recoverable_error("ParseUrl() port number out of range\n");
            return false;
        }
    }

    // Parse path
    size_t pathStart = urlStr.find(L'/', hostEnd);
    if (pathStart != std::wstring::npos) {
        std::wstring pathStr = urlStr.substr(pathStart);
        if (pathStr.length() >= sizeof(components.path) / sizeof(wchar_t)) {
            logger->recoverable_error("ParseUrl() path too long\n");
            return false;
        }
        wcsncpy_s(components.path, sizeof(components.path) / sizeof(wchar_t), 
                  pathStr.c_str(), _TRUNCATE);
    } else {
        wcscpy_s(components.path, sizeof(components.path) / sizeof(wchar_t), L"/");
    }

    return true;
}


std::string Util::getTempDirectory() {
    std::string tempPath;
    
    #if defined(_WIN32) || defined(_WIN64)
        // Windows implementation
        char buffer[MAX_PATH];
        DWORD result = GetTempPathA(MAX_PATH, buffer);
        if (result != 0) {
            tempPath = std::string(buffer);
            // Remove trailing backslash if present
            if (!tempPath.empty() && tempPath.back() == '\\') {
                tempPath.pop_back();
            }
        } else {
            // Fallback if GetTempPath fails
            char* tempEnv = nullptr;
            size_t requiredSize;
            _dupenv_s(&tempEnv, &requiredSize, "TEMP");
            if (tempEnv) {
                tempPath = tempEnv;
                free(tempEnv);
            } else {
                char* tmpEnv = nullptr;
                _dupenv_s(&tmpEnv, &requiredSize, "TMP");
                if (tmpEnv) {
                    tempPath = tmpEnv;
                    free(tmpEnv);
                } else {
                    tempPath = "C:\\TEMP";
                }
            }
        }
    #else
        // Linux/Unix implementation
        if (const char* env = std::getenv("TMPDIR")) {
            tempPath = env;
        } else if (const char* env = std::getenv("TMP")) {
            tempPath = env;
        } else if (const char* env = std::getenv("TEMP")) {
            tempPath = env;
        } else {
            tempPath = "/tmp";
        }
        
        // Remove trailing slash if present
        if (!tempPath.empty() && tempPath.back() == '/') {
            tempPath.pop_back();
        }
    #endif
    
    return tempPath;
}

char Util::getPathSeparator() {
    return std::filesystem::path::preferred_separator;
}

/**
 * @brief Determines the appropriate log file directory based on write permissions
 * 
 * @param logFileName The name of the log file (without path)
 * @return std::string The full path to the log file
 */
std::string Util::getAppropriateLogPath(const std::string& logFileName) {
    std::string logPath;
    
    // First, try to use the application directory
    bool hasWriteAccess = false;
    
    // Get the application directory path using buffer-based approach
    wchar_t pathBuffer[MAX_PATH] = {0};
    bool pathOk = Util::getThisPath(pathBuffer, MAX_PATH, true); // With trailing separator
    std::string appDir;
    
    if (pathOk) {
        // Convert wchar_t buffer to string
        int bufferSize = WideCharToMultiByte(CP_UTF8, 0, pathBuffer, -1, nullptr, 0, nullptr, nullptr);
        if (bufferSize > 0) {
            appDir.resize(bufferSize);
            WideCharToMultiByte(CP_UTF8, 0, pathBuffer, -1, &appDir[0], bufferSize, nullptr, nullptr);
            appDir.resize(strlen(appDir.c_str())); // Adjust for null terminator
        }
        
        // Create a test file with unique name to check write access
        std::string testFileName = appDir + "writetest_" + std::to_string(GetTickCount64()) + ".tmp";
        FILE* fp = nullptr;
        errno_t err = fopen_s(&fp, testFileName.c_str(), "w");
        if (fp && err == 0) {
            hasWriteAccess = true;
            fclose(fp);
            remove(testFileName.c_str()); // Clean up
            
            // If we have write access, use the application directory
            logPath = appDir + logFileName;
        }
    }
    
    // If we don't have write access or app directory retrieval failed,
    // fall back to the temp directory
    if (!hasWriteAccess) {
        std::string tempDir = Util::getTempDirectory();
        char separator = Util::getPathSeparator();
        
        // Ensure temp directory path ends with separator
        if (!tempDir.empty() && tempDir.back() != separator) {
            tempDir += separator;
        }
        
        logPath = tempDir + logFileName;
    }
    
    return logPath;
}