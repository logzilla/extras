/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#include "stdafx.h"
#include "Logger.h"
#include <algorithm>
#include <clocale>
#include <codecvt>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <locale>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <string>
#include <vector>
#include <wchar.h>

#include "Util.h"

using namespace std;

vector<string> Logger::LOGLEVEL_ABBREVS_WITHBRACKET;

Logger* Logger::singleton() {
	static Logger instance;
	return &instance;
}

//Logger* Logger::singleton() {
//	static Logger* instance = nullptr;
//	static std::mutex mutex;
//
//	if (instance == nullptr) {
//		std::lock_guard<std::mutex> lock(mutex);
//		if (instance == nullptr) {
//			instance = new Logger();
//		}
//	}
//	return instance;
//}


Logger::Logger()
	: log_path_and_filename_(DEFAULT_LOG_FILENAME),
	log_events_to_file_(false),
	is_unittest_running_(-1),
	fatal_error_handler_(nullptr),
	log_message_buffer_{ 0 },
	logger_lock_() {
	int l = 0;
	LOGLEVEL_ABBREVS_WITHBRACKET.resize(static_cast<int>(LogLevel::FORCE) + 1);
	do {
		LOGLEVEL_ABBREVS_WITHBRACKET[l] = std::string(LOGLEVEL_ABBREVS[l]) + "] ";
	} while (l++ != static_cast<int>(LogLevel::FORCE));
}

bool Logger::log(const LogLevel log_level, const char* format, ...) {

	bool log_level_force = log_level == FORCE;
	bool current_log_level_nolog = singleton()->current_log_level_ == NOLOG;
	bool log_level_always = log_level == ALWAYS;
	bool log_level_greater = log_level >= singleton()->current_log_level_;
	bool do_log = log_level_force
		|| (current_log_level_nolog && log_level_always)
		|| log_level_greater;

	if (!do_log) {
		return true;
	}
	bool result;

	//Logger* logger = singleton();
	//// Access the mutex through the logger instance
	//logger->logger_lock_.lock();
	//// ...
	std::unique_lock<std::mutex> lock(singleton()->logger_lock_);

	char dt_buf[40];
	time_t now = time(0);
	struct tm tstruct;
	localtime_s(&tstruct, &now);
	const char* log_abbrev = (log_level <= FORCE ? LOGLEVEL_ABBREVS_WITHBRACKET[static_cast<int>(log_level)].c_str() : nullptr);
	strftime(dt_buf, sizeof(dt_buf), "[%Y%m%d.%H%M%S ", &tstruct);
	switch (singleton()->log_destination_) {
	case DEST_CONSOLE:
		result = singleton()->logToConsole(dt_buf);
		result = singleton()->logToConsole(log_abbrev);
		break;
	case DEST_FILE:
		result = singleton()->logToFile(dt_buf);
		result = singleton()->logToFile(log_abbrev);
		break;
	case DEST_CONSOLE_AND_FILE:
		result = singleton()->logToConsoleAndFile(dt_buf);
		result = singleton()->logToConsoleAndFile(log_abbrev);
		break;
	default:
		result = false;
		break;
	}

	if (result) {

		va_list args;
		va_start(args, format);
		int num_chars = vsnprintf_s(
			singleton()->log_message_buffer_,
			sizeof(singleton()->log_message_buffer_),
			_TRUNCATE,
			format,
			args
		);
		va_end(args);

		bool result = true;
		switch (singleton()->log_destination_) {
		case DEST_CONSOLE:
			result = singleton()->logToConsole(singleton()->log_message_buffer_);
			break;
		case DEST_FILE:
			result = singleton()->logToFile(singleton()->log_message_buffer_);
			break;
		case DEST_CONSOLE_AND_FILE:
			result = singleton()->logToConsoleAndFile(singleton()->log_message_buffer_);
			break;
		}
	}
	return result;
}


bool Logger::log_no_datetime(const LogLevel log_level, const char* format, ...) {
	if (log_level < singleton()->current_log_level_) {
		return true;
	}
	std::unique_lock<std::mutex> lock(singleton()->logger_lock_);

	va_list args;
	va_start(args, format);
	int num_chars = vsnprintf_s(
		singleton()->log_message_buffer_,
		sizeof(singleton()->log_message_buffer_),
		_TRUNCATE,
		format,
		args
	);
	va_end(args);

	bool result = true;
	switch (singleton()->log_destination_) {
	case DEST_CONSOLE:
		result = singleton()->logToConsole(singleton()->log_message_buffer_);
		break;
	case DEST_FILE:
		result = singleton()->logToFile(singleton()->log_message_buffer_);
		break;
	case DEST_CONSOLE_AND_FILE:
		result = singleton()->logToConsoleAndFile(singleton()->log_message_buffer_);
		break;
	}
	return result;
}

void Logger::setLogLevel(const LogLevel log_level) {
	// std::lock_guard<std::mutex> guard(singleton()->logger_lock_);
	singleton()->current_log_level_ = log_level;
	if (log_level != NOLOG) {
		always("Log level set to: %s\n", LOGLEVEL_ABBREVS[(int) log_level]);
	}
}

Logger::LogLevel Logger::getLogLevel() {
	return singleton()->current_log_level_;
}

void Logger::setLogDestination(LogDestination log_destination) {
	// std::lock_guard<std::mutex> guard(singleton()->logger_lock_);
	singleton()->log_destination_ = log_destination;
}

bool Logger::logToConsole(const char* log_message_cstring) const {
	if (log_message_cstring) {
		printf("%s", log_message_cstring);
	}
	return true;
}

bool Logger::logToFile(const char* log_message_cstring) const {
	if (log_message_cstring) {
		FILE* logfile;
		_wfopen_s(&logfile, log_path_and_filename_.c_str(), L"a");
		if (!logfile) {
			return false;
		}
		fputs(log_message_cstring, logfile);
		fclose(logfile);
	}
	return true;
}

bool Logger::logToConsoleAndFile(const char* log_message_cstring) const {
	logToConsole(log_message_cstring);
	logToFile(log_message_cstring);
	return true;
}


void Logger::setLogFile(const wstring const_log_path_and_filename) {
	// if no spec given then use the default
	wstring log_path_and_filename;
	if (const_log_path_and_filename.length() < 2) {
		log_path_and_filename = DEFAULT_LOG_FILENAME;
	}
	else {
		log_path_and_filename = const_log_path_and_filename;
	}
	// if no drive and no leading backslash then use the exe dir
	if (log_path_and_filename.substr(1, 1) != L":"
		&& log_path_and_filename.substr(0, 1) != L"\\") {
		wstring path_str = Util::getThisPath();
		log_path_and_filename = path_str + L"\\" + log_path_and_filename;
	}
	singleton()->log_path_and_filename_ = log_path_and_filename;
}


void Logger::getDateTimeStr(char* buf, int bufsize) {
	time_t     now = time(0);
	struct tm  tstruct;
	localtime_s(&tstruct, &now);
	strftime(buf, bufsize, "%Y-%m-%d.%X", &tstruct);
}


bool Logger::isUnittestRunning() {
	if (singleton()->is_unittest_running_ == -1) {
		char value[16];  // Buffer to store the environment variable value
		size_t size = sizeof(value);
		errno_t err = getenv_s(&size, value, "UNITTEST_RUNNING");

		if (err != 0 || size == 0) {
			singleton()->is_unittest_running_ = 0;
			// Failed to retrieve the variable, assuming not running
		}
		else {
			// Compare the value and set is_unittest_running_ appropriately
			singleton()->is_unittest_running_ = (strcmp(value, "1") == 0) ? 1 : 0;
		}
	}
	return singleton()->is_unittest_running_ == 1;
}


string Logger::getUnitTestLog() {
#if _UNITTEST
	string result;
	for (const auto& msg : singleton()->unit_test_messages_) {
		result += msg;
	}
	singleton()->unit_test_messages_.clear();
	return result;
#else
	return string();
#endif
}

int Logger::writeToFile(const char* filename, bool append, const char* format, ...) {
	va_list args;
	va_start(args, format);
	int num_chars = vsnprintf_s(
		singleton()->log_message_buffer_,
		sizeof(singleton()->log_message_buffer_),
		_TRUNCATE,
		format,
		args
	);
	va_end(args);
	if (num_chars < 1) {
		return num_chars;
	}
	FILE* output_file;
	fopen_s(&output_file, filename, append ? "a" : "w");
	if (output_file == NULL) {
		return -1;
	}
	int num_chars_written;
	for (num_chars_written = 0; num_chars_written < num_chars && singleton()->log_message_buffer_[num_chars_written] != 0; ++num_chars_written) {
		if (fputc(singleton()->log_message_buffer_[num_chars_written], output_file) == EOF) {
			fclose(output_file);
			return num_chars_written;
		}
	}
	fclose(output_file);
	return num_chars_written;
}


void Logger::fatal(const char* format, ...) {
	va_list args_list;
	va_start(args_list, format);

	// Format the message into the buffer
	char formatted_message[MAX_LOGMSG_LENGTH];
	vsnprintf(formatted_message, MAX_LOGMSG_LENGTH, format, args_list);

	// Log the formatted message with FATAL log level
	log(FATAL, "%s", formatted_message);

	// Call the fatal error handler if it's set
	if (singleton()->fatal_error_handler_) {
		singleton()->fatal_error_handler_(formatted_message);
	}

	va_end(args_list);
}

