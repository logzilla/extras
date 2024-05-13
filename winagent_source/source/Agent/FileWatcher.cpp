/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#include "stdafx.h"

#include "FileWatcher.h"
#include "OStreamBuf.h"
#include <ostream>
#include <stdio.h>
#include <windows.h>
#include <winnt.h>
#include "Logger.h"
#include "Util.h"


using namespace Syslog_agent;

FileWatcher::FileWatcher(
	Configuration& config,
	const wchar_t* filename,
	int max_line_length,
	const char* program_name,
	const char* host_name,
	int severity,
	int facility
) 
	: config_(config), 
	filename_(filename),
	max_line_length_(max_line_length),
	program_name_(program_name),
	host_name_(host_name),
	severity_(severity),
	facility_(facility),
	last__buffer_position_(-1),
	last_file_position_(-1),
	last_file_size_(0),
	last_char_read_(0),
	num_prebuffer_chars_(0)
{
	size_t num_bytes_used;
	errno_t result = wcstombs_s(&num_bytes_used, filename_multibyte_, sizeof(filename_multibyte_), 
		filename_.c_str(), filename_.length());
	if (result != ERROR_SUCCESS || num_bytes_used < 1) {
		filename_multibyte_[0] = 0;
		throw Result(ResultCodes::BadFileName, "FileWatcher()", "couldn't parse filename");
	}
	string filename_escaped(filename_multibyte_, num_bytes_used);
	Util::replaceAll(filename_escaped, "\\", "\\\\");
	strcpy_s(filename_multibyte_escaped_, sizeof(filename_multibyte_escaped_), filename_escaped.c_str());
	read_buffer_.resize(max_line_length + READ_BUF_SIZE);
	buffer_write_start_ = read_buffer_.data() + max_line_length;
	num_prebuffer_chars_ = 0;
	message_buffer_.resize(max_line_length + JSON_HEADERS_SIZE);

	readToLastLine();
}

HANDLE FileWatcher::openLogFile() {
	HANDLE file_handle = CreateFileA(
		filename_multibyte_,
		GENERIC_READ,
		FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
		NULL
	);
	if (file_handle == INVALID_HANDLE_VALUE) {
		throw Result::ResultLog(ResultCodes::FailOpenFile, Logger::DEBUG2, "FileWatcher::openLogFile()", "could not open file %s, error %d", filename_multibyte_, GetLastError());
	}
	return file_handle;
}

Result FileWatcher::readToLastLine() {

	HANDLE file_handle;
	try {
		file_handle = openLogFile();
	}
	catch (Result result) {
		// if we couldn't open the file then there's no last line to read to, which is fine
		Logger::debug2("FileWatcher::readToLastLine() could not open file %s\n", filename_multibyte_);
		return Result(ResultCodes::Success);
	}
	// get new file size & position
	LARGE_INTEGER current_file_size;
	if (0 != GetFileSizeEx(file_handle, &current_file_size)) {
		last_file_size_ = current_file_size.LowPart;
	}

	// seek back from end
	LARGE_INTEGER file_pos;
	if (last_file_size_ > max_line_length_) {
		file_pos.QuadPart = 0 - max_line_length_;
		if (0 == SetFilePointerEx(file_handle, file_pos, NULL, FILE_END)) {
			CloseHandle(file_handle);
			return Result::ResultLog(ResultCodes::FailReadFile, Logger::DEBUG2, 
				"could not seek log file %s, error %d", filename_multibyte_, GetLastError());
		}
	}

	DWORD num_bytes_read;

	if (0 == ReadFile((HANDLE)file_handle, read_buffer_.data(), max_line_length_, &num_bytes_read, NULL))
	{
		CloseHandle(file_handle);
		return Result::ResultLog(ResultCodes::FailReadFile, Logger::DEBUG2, 
			"could not read log file %s, error %d", filename_multibyte_, GetLastError());
	}
	if (num_bytes_read < 1)
	{
		// if there's no data to read then there's no last line to read to, which is fine
		CloseHandle(file_handle);
		return Result(ResultCodes::Success);
	}

	file_pos.QuadPart = 0;
	LARGE_INTEGER new_file_pos;
	SetFilePointerEx(file_handle, file_pos, &new_file_pos, FILE_CURRENT);
	last_file_position_ = new_file_pos.LowPart;

	CloseHandle(file_handle);

	// we read in some block of data presumably including a line break
	// check backwards in read block to first cr or lf
	int p;
	for (p = num_bytes_read; p > 0; --p) {
		char c = *(read_buffer_.data() + p - 1);
		if (c == LINEBREAK || c == CARRIAGERETURN) {
			break;
		}
	}

	// carriage return found at p (or none found so from beginning)
	// move this so line is contiguous with read buffer
	num_prebuffer_chars_ = num_bytes_read - p;
	if (num_prebuffer_chars_ > 0) {
		char* line_start = read_buffer_.data() + p;
		char* prebuffer_start = read_buffer_.data() + max_line_length_ - num_prebuffer_chars_;
		memmove(prebuffer_start, line_start, num_prebuffer_chars_);
	}

	return Result(ResultCodes::Success);
}

void FileWatcher::processLine(const char* line_cstr) {

	OStreamBuf<char> ostream_buffer(message_buffer_.data(), message_buffer_.size());
	std::ostream stream_output(&ostream_buffer);

	for (int servernum = 0; servernum < 2; servernum++) {

		// Reset the ostream to be used again
		stream_output.clear(); // Clear any error flags
		ostream_buffer.pubsetbuf(message_buffer_.data(), message_buffer_.size()); // Reset buffer
		ostream_buffer.pubseekpos(0); // Optionally, reset position to the beginning

		int log_format = (servernum == 0 ? config_.getPrimaryLogformat() : config_.getSecondaryLogformat());

		if (log_format == SharedConstants::LOGFORMAT_HTTPPORT) {

			stream_output << "{ "
				<< "\"program\" : \"" << program_name_ << "\", "
				<< "\"host\": \"" << host_name_ << "\", "
				<< "\"severity\": " << severity_ << ", "
				<< "\"facility\": " << facility_ << ", "
				<< "\"message\": \"" << line_cstr << "\", "
				<< "\"extra_fields\": { "
				<< "\"_source_tag\": \"windows_agent\", "
				<< "\"log_type\": \"file\", "
				<< "\"file\": \"" << filename_multibyte_escaped_ << "\" }"
				<< " }" << '\n' << '\0';
		}
		else {
			stream_output << "{ \"_source_type\": \"WindowsAgent\", \"_log_type\": \"file\", \"program\" : \""
				<< program_name_ << "\", \"host\": \"" << host_name_ << "\", \"severity\": " << severity_
				<< ", \"facility\": " << facility_ << ", \"file\": \"" << filename_multibyte_escaped_
				<< "\", \"message\": \"" << line_cstr << "\" }" << '\n' << '\0';
		}

		shared_ptr<MessageQueue> msg_queue = (servernum == 0 ? primary_message_queue_ : secondary_message_queue_);
		msg_queue->enqueue(reinterpret_cast<const char*>(message_buffer_.data()), 
			static_cast<uint32_t>(strlen(message_buffer_.data())));

		if (!config_.hasSecondaryHost()) {
			break;
		}
	}
}


Result FileWatcher::process() {

	int lines_processed = 0;
	HANDLE file_handle;
	try {
		file_handle = openLogFile();
	}
	catch (Result result) {
		return result;
	}

	LARGE_INTEGER current_file_size;
	if (0 == GetFileSizeEx(file_handle, &current_file_size)) {
		return Result::ResultLog(ResultCodes::FailOpenFile, Logger::DEBUG2, 
			"could not check file size %s, error %d", filename_multibyte_, GetLastError());
	}

	if (current_file_size.QuadPart == last_file_size_) {
		// assume file hasn't changed
		CloseHandle(file_handle);
		return Result(ResultCodes::NoNewData);
	}

	// file must have changed

	LARGE_INTEGER file_pos;
	if (last_file_position_ > 0) {
		file_pos.QuadPart = last_file_position_;
		SetFilePointerEx(file_handle, file_pos, NULL, FILE_BEGIN);
	}

	DWORD num_bytes_read = 0;
	char* buffer_start = read_buffer_.data();

	int current_line_offset = max_line_length_ - num_prebuffer_chars_;

	while (true) {
		BOOL readfile_result = ReadFile((HANDLE)file_handle, buffer_write_start_, READ_BUF_SIZE, &num_bytes_read, NULL);

		if (!readfile_result || num_bytes_read < 1) break;

		uint32_t p;
		for (p = current_line_offset; p < max_line_length_ + num_bytes_read; ++p) {
			char cur_char = *(buffer_start + p);
			// if we had CR LF, or LF CR, this must have happened at the start of a line
			// (since we ended the last line at either)
			// so push the line start forward one and continue
			if (
				(cur_char == CARRIAGERETURN && last_char_read_ == LINEBREAK)
				|| (cur_char == LINEBREAK && last_char_read_ == CARRIAGERETURN)
				)
			{
				current_line_offset++;
				last_char_read_ = cur_char;
			}
			else if (cur_char == LINEBREAK || cur_char == CARRIAGERETURN) {
				last_char_read_ = cur_char;
				*(buffer_start + p) = 0;

				++lines_processed;
				processLine(buffer_start + current_line_offset);
				current_line_offset = p + 1;
				num_prebuffer_chars_ = 0;
			}
			else {
				last_char_read_ = cur_char;
			}
		}
		if (
			(p >= max_line_length_ + num_bytes_read)
			&& (last_char_read_ != LINEBREAK)
			&& (last_char_read_ != CARRIAGERETURN)
			)
		{
			// we reached end of read buffer while still in the middle of a line
			// copy this partial line to the beginning so that we can
			// read in the next block from the file and parse both this
			// partial line and the new block
			num_prebuffer_chars_ = p - current_line_offset;
			int next_parse_offset = max_line_length_ - num_prebuffer_chars_;
			memmove(buffer_start + next_parse_offset, buffer_start + current_line_offset, num_prebuffer_chars_);
			current_line_offset = next_parse_offset;
		}
	}

	file_pos.QuadPart = 0;
	LARGE_INTEGER new_file_pos;
	SetFilePointerEx(file_handle, file_pos, &new_file_pos, FILE_CURRENT);
	last_file_position_ = new_file_pos.LowPart;

	if (0 == GetFileSizeEx(file_handle, &current_file_size)) {
		CloseHandle(file_handle);
		return Result::ResultLog(ResultCodes::FailOpenFile, Logger::DEBUG2, "could not check file size %s", Util::wstr2str(filename_).c_str());
	}
	last_file_size_ = current_file_size.LowPart;

	CloseHandle(file_handle);

	Logger::debug2("FileWatcher::process() success, file %s, %d lines processed\n", filename_multibyte_, lines_processed);
	return Result(ResultCodes::Success);
}

