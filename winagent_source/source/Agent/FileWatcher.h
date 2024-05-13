/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include "Configuration.h"
#include "MessageQueue.h"
#include "Result.h"
#include "SyslogAgentSharedConstants.h"

using namespace std;

namespace Syslog_agent {

	class FileWatcher {

	public:
		enum ResultCodes { Success = 0, NoNewData = 1, BadFileName = 2, FailOpenFile, FailReadFile };
		static constexpr char LINEBREAK = '\n';
		static constexpr char CARRIAGERETURN = '\r';
		static constexpr int READ_BUF_SIZE = 4000;
		FileWatcher(
			Configuration& config,
			const wchar_t* filename,
			int max_line_length,
			const char* program_name,
			const char* host_name,
			int severity,
			int facility
		);
		Result process();

	private:
		static constexpr int JSON_HEADERS_SIZE = 200;
		int max_line_length_;
		vector<char> read_buffer_;
		vector<char> message_buffer_;
		wstring filename_;
		char filename_multibyte_[2000];
		char filename_multibyte_escaped_[2000];
		int last__buffer_position_;
		LONG last_file_position_;
		LONGLONG last_file_size_;
		char* buffer_write_start_;
		int num_prebuffer_chars_;
		char last_char_read_;
		string program_name_;;
		string host_name_;
		int severity_;
		int facility_;
		Configuration& config_;
		shared_ptr<MessageQueue> primary_message_queue_;
		shared_ptr<MessageQueue> secondary_message_queue_;


		HANDLE openLogFile();
		Result readToLastLine();
		void processLine(const char* line_cstr);
	};
};
