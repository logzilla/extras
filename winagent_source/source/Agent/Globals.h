/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once
#include <memory>
#include <mutex>
#include <vector>
#include "BitmappedObjectPool.h"

/*
I realize globals / singletons are denigrated but given that this app
is supposed to run for long periods of time, memory churn and heap
fragmentation are of concern.  Consequently this class is used to 
store objects that should persist and not be continually allocated/
deallocated.
*/

namespace Syslog_agent {
	class Globals {
	public:
		static constexpr unsigned int MESSAGE_BUFFER_SIZE = 132000;
		static constexpr unsigned int BUFFER_CHUNK_SIZE = 12;
		static constexpr unsigned int PERCENT_SLACK = -1;
		static void Initialize();
		char* getMessageBuffer(char *debug_text);
		void releaseMessageBuffer(char* debug_text, char* buffer);
		static Globals* instance() {
			if (instance_ == nullptr)
				Initialize();
			return instance_.get();
		}
		int getMessageBufferSize() const { return message_buffers_->countBuffers(); }
	private:
		Globals(
			int buffer_chunk_size,
			int percent_slack);
		static std::unique_ptr<Globals> instance_;
		std::unique_ptr<BitmappedObjectPool<char[MESSAGE_BUFFER_SIZE]>> message_buffers_;
#ifdef DEBUG
		std::mutex debug_logging_;
#endif

	};
}
