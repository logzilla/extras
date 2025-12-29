#include "pch.h"

/*
SyslogAgent: a syslog agent for Windows
Copyright 2021 Logzilla Corp.
*/

#include <memory>
#include "Globals.h"
#include "../Infrastructure/Logger.h"

using namespace std;

namespace Syslog_agent {

    std::unique_ptr<Globals> Globals::instance_ = nullptr;
    std::once_flag Globals::init_flag_;

    Globals::Globals(int buffer_chunk_size, int percent_slack) {
        auto logger = LOG_THIS;
        message_buffers_ = make_unique<BitmappedObjectPool<char[MESSAGE_BUFFER_SIZE]>>(
            buffer_chunk_size, percent_slack);
        sid_cache_ = make_unique<SidCache>(SID_CACHE_INITIAL_SIZE, SID_CACHE_SLACK_PERCENT);

        // Initialize the resource monitor for global message buffers with specified thresholds
        message_buffer_monitor_ = make_unique<ResourceMonitor>(
            "GlobalMessageBuffers",
            "global message buffer threshold hit: %d",
            std::make_tuple(0, 0),        // Debug: disabled
            std::make_tuple(0, 0),        // Verbose: disabled
            std::make_tuple(10, 100),     // Info: log every 10 allocations up to 100
            std::make_tuple(100, 1000),   // Warning: log every 100 allocations up to 1000
            std::make_tuple(1000, 10000), // RecoverableError: log every 1000 allocations up to 10000
            std::make_tuple(100000, 9999999), // CriticalError: log every 100000 allocations up to 9999999
            std::make_tuple(0, 0),        // FatalError: disabled
            20                           // 20% hysteresis
        );

        // Associate the monitor with the message buffer pool
        message_buffers_->setResourceMonitor(message_buffer_monitor_.get());
    }

    void Globals::Initialize() {
        auto logger = LOG_THIS;
        std::call_once(init_flag_, []() {
            try {
                instance_.reset(new Globals(BUFFER_CHUNK_SIZE, PERCENT_SLACK));
            }
            catch (const std::exception& e) {
                auto init_logger = LOG_THIS;
                init_logger->fatal("Failed to initialize Globals: %s\n", e.what());
                throw; // Re-throw to prevent use of uninitialized instance
            }
            });
    }

    Globals* Globals::instance() {
        Initialize();
        return instance_.get();
    }

    char* Globals::getMessageBuffer(const char* debug_identifier) {
        auto logger = LOG_THIS;
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        auto* buffer = message_buffers_->getAndMarkNextUnused();
        if (!buffer) {
            if (debug_identifier) {
                logger->recoverable_error("Failed to allocate message buffer for %s\n", debug_identifier);
            }
            else {
                logger->recoverable_error("Failed to allocate message buffer\n");
            }
            return nullptr;
        }

        // The resource monitor is notified automatically via the setResourceMonitor call
        // in the constructor, so no need to notify it manually here

        return static_cast<char*>(*buffer);
    }

    void Globals::releaseMessageBuffer(char* buffer) {
        if (!buffer) return;

        std::lock_guard<std::mutex> lock(buffer_mutex_);
        auto ptr = reinterpret_cast<char(*)[MESSAGE_BUFFER_SIZE]>(buffer);
        message_buffers_->markAsUnused(ptr);

        // The resource monitor is notified automatically via the setResourceMonitor call
        // in the constructor, so no need to notify it manually here
    }

    //int Globals::getMessageBufferSize() const {
    //    std::lock_guard<std::mutex> lock(buffer_mutex_);
    //    return static_cast<int>(message_buffers_->countBuffers());
    //}

    DWORD Globals::LookupUserSid(const char* sidString, char* domainBuffer, DWORD domainBufSize,
        char* nameBuffer, DWORD nameBufSize) {
        if (!sid_cache_) {
            return ERROR_NOT_READY; // Cache not initialized
        }

        if (!sid_cache_) {
            return ERROR_INVALID_PARAMETER; // Or another appropriate error
        }

        auto lookup_result = LookupAccountFromSidWithCache(*sid_cache_, sidString, domainBuffer, domainBufSize, nameBuffer, nameBufSize);
        return lookup_result;
    }

}