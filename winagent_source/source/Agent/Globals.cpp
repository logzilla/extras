/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#include "stdafx.h"
#include <memory>
#include "Globals.h"

using namespace std;

namespace Syslog_agent {

	std::unique_ptr<Globals> Globals::instance_ = nullptr;

	Globals::Globals(int buffer_chunk_size, int percent_slack) {
		message_buffers_ 
			= make_unique <BitmappedObjectPool<char[MESSAGE_BUFFER_SIZE]>>(buffer_chunk_size, percent_slack);
	}

	void Globals::Initialize() {
		if (instance_ != nullptr)
			return;

		auto temp_new_ptr = new Globals(BUFFER_CHUNK_SIZE, PERCENT_SLACK);
		instance_.reset(temp_new_ptr);
	}

	char* Globals::getMessageBuffer(char* debug_text) {
		auto result = reinterpret_cast<char*>(message_buffers_->getAndMarkNextUnused());
		return result;
	}

	void Globals::releaseMessageBuffer(char* debug_text, char* buffer) {
		auto ptr = (char(*)[MESSAGE_BUFFER_SIZE]) buffer;
		message_buffers_->markAsUnused(ptr);
	}

}