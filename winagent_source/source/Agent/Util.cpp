/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#include "stdafx.h"
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <clocale>
#include <codecvt>
#include <cwctype>
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
#include <windows.h>
#include <Psapi.h>
#include <TlHelp32.h>

#include "Util.h"


using namespace std;


void Util::toPrintableAscii(char* destination, int destination_count, 
	const wchar_t* source, char space_replacement) {
	int i;
	for (i = 0; i < destination_count - 1; i++) {
		if (source[i] == 0) break;
		if (source[i] < 32 || source[i] > 126) destination[i] = '?';
		else if (source[i] == 32) destination[i] = space_replacement;
		else destination[i] = static_cast<char>(source[i]);
	}
	destination[i] = 0;
}


std::string Util::wstr2str(const std::wstring& wstr)
{
	using convert_typeX = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> converterX;

	return converterX.to_bytes(wstr);
}


std::string Util::wstr2str_truncate(const std::wstring& wstr) {
	std::string result(wstr.length(), 0);
	std::transform(wstr.begin(), wstr.end(), std::back_inserter(result), [](wchar_t c) {
		return (char)c;
		});
	return result;
}


wstring Util::getThisPath()
{
	return getThisPath(false);
}


wstring Util::getThisPath(bool with_trailing_backslash)
{

	TCHAR buf[1024];
	DWORD length = GetModuleFileName(NULL, buf, 1024);
	// link to this library fails...
	//#if (NTDDI_VERSION >= NTDDI_WIN8)
	//	PathCchRemoveFileSpec(dest, destSize);
	//#else
	//	if (MAX_PATH > destSize) return NULL;
	//	PathRemoveFileSpec(dest);
	//#endif
	// so we do it this way:
	wstring module_filename_wstr = wstring(buf, length);
	size_t last_pos = module_filename_wstr.find_last_of(L'\\');
	if (last_pos == string::npos || last_pos < 1) {
		return wstring();
	}
	return module_filename_wstr.substr(0, last_pos) 
		+ (with_trailing_backslash ? L"\\" : L"");
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
{	// hash _Keyval to size_t value by pseudorandomizing transform
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
		char escaped_char = 0;
		char cur_char = input_buffer[i];
		switch (cur_char) {
		case 9:
			escaped_char = 't';
			break;
		case 10:
			escaped_char = 'n';
			break;
		case 13:
			escaped_char = 'r';
			break;
		case '\\':
			escaped_char = '\\';
			break;
		case '"':
			escaped_char = '"';
			break;
		}
		if (escaped_char != 0) {
			output_buffer[output_pos++] = '\\';
			output_buffer[output_pos++] = escaped_char;
		}
		else {
			output_buffer[output_pos++] = cur_char;
		}
	}
	output_buffer[output_pos] = 0;
	return output_pos + 1;
}


static vector<std::wstring> wstringSplit(
	const std::wstring& delimited_string, 
	const wchar_t delimiter) {
	wstringstream ss(delimited_string);
	wstring substr;
	vector<wstring> elems;
	while (ss.good())
	{
		getline(ss, substr, delimiter);
		elems.push_back(substr);
	}
	return elems;
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

void EnumerateOpenFileHandles(DWORD processId)
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

void EnumerateOpenTcpSockets(DWORD processId)
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


std::wstring Util::toLowercase(const std::wstring& input) {
	std::wstring result = input;
	std::transform(result.begin(), result.end(), result.begin(),
		[](wchar_t c) { return std::towlower(c); });
	return result;
}


std::string Util::toLowercase(const std::string& input) {
	std::string result = input;
	std::transform(result.begin(), result.end(), result.begin(),
		[](unsigned char c) { return std::tolower(c); });
	return result;
}


int64_t Util::getUnixTimeMilliseconds() {
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);  // Retrieves the current system time in UTC

	// Combine high and low parts to form a 64-bit value
	ULARGE_INTEGER li;
	li.LowPart = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;

	// Convert FILETIME (100-nanoseconds since January 1, 1601) to 
	// Unix epoch time in milliseconds
	int64_t unixTimeMilliseconds = (li.QuadPart - 116444736000000000LL) / 10000;

	return unixTimeMilliseconds;
}



int Util::compareSoftwareVersions(const std::string& version_a, 
	const std::string& version_b) {
	std::vector<int> parts_a = splitVersion(version_a);
	std::vector<int> parts_b = splitVersion(version_b);

	// Find the maximum length of version parts
	size_t maxLength = (std::max)(parts_a.size(), parts_b.size());

	for (size_t i = 0; i < maxLength; i++) {
		// Get the current part for each version.
		// If the version doesn't have this part, treat it as '0'
		int part_a = i < parts_a.size() ? parts_a[i] : 0;
		int part_b = i < parts_b.size() ? parts_b[i] : 0;

		// Compare the current parts
		if (part_a < part_b)
			return -1;
		if (part_a > part_b)
			return 1;
	}

	// If all parts are equal
	return 0;
}


std::vector<int> Util::splitVersion(const std::string& version) {
	std::vector<int> parts;
	std::istringstream ss(version);
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
					parts.push_back(0); 
						// Default to zero if conversion fails
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

