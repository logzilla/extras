/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once

#include <cctype>
#include <cwctype>
#include <string>
#include <vector>
#include <wchar.h>

class Util {
public:
	static void toPrintableAscii(char* destination, int destination_count, 
		const wchar_t* source, char space_replacement);
	static std::wstring getThisPath();
	static std::wstring getThisPath(bool with_trailing_backslash);
	static std::string wstr2str(const std::wstring& wstr);
	static std::string wstr2str_truncate(const std::wstring& wstr);
	static std::string readFileAsString(const char* filename);
	static std::string readFileAsString(const wchar_t* filename);
	static void replaceAll(std::string& str, const std::string& from, const std::string& to);
	static size_t hashWstring(const std::wstring& _Keyval);
	static int jsonEscape(char* input_buffer, char* output_buffer, int output_buffer_length);
	static std::vector<std::wstring> wstringSplit(const std::wstring& delimited_string, 
		const wchar_t delimiter);
	static bool copyFile(const wchar_t* const source_filename, const wchar_t* const dest_filename);
	static std::wstring toLowercase(const std::wstring& input);
	static std::string toLowercase(const std::string& input);
	static int64_t getUnixTimeMilliseconds();
	static int compareSoftwareVersions(const std::string& version_a, const std::string& version_b);
	static std::vector<int> splitVersion(const std::string& version);

};

