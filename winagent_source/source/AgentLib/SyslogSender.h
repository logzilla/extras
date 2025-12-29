#pragma once
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <Windows.h>
#include <WinSvc.h>

#include "INetworkClient.h"
#include "MessageQueue.h"
#include "MessageBatcher.h"
#include "../Infrastructure/WindowsTimer.h"
#include "framework.h"

// Ensure Logger is used from global namespace to avoid ambiguity
using ::Logger;

namespace Syslog_agent {

    class AGENTLIB_API SyslogSender {
    public:
        static constexpr uint32_t MAX_MESSAGE_SIZE = 65536;         // Maximum size of a message batch in bytes
        static constexpr uint32_t SEND_BUFFER_SIZE = 8 * 1024 * 1024; // Size of the send buffer in bytes

        SyslogSender(
            std::shared_ptr<MessageQueue> primary_queue,
            std::shared_ptr<MessageQueue> secondary_queue,
            std::shared_ptr<INetworkClient> primary_network_client,
            std::shared_ptr<INetworkClient> secondary_network_client,
            std::shared_ptr<MessageBatcher> primary_batcher,
            std::shared_ptr<MessageBatcher> secondary_batcher,
            uint32_t max_batch_size,
            uint32_t max_batch_age);

        ~SyslogSender() = default;

        // Prevent copying
        SyslogSender(const SyslogSender&) = delete;
        SyslogSender& operator=(const SyslogSender&) = delete;

        void run() const;

        void requestStopAndNotify() {
            auto logger = LOG_THIS;
            logger->debug2("SyslogSender::requestStopAndNotify()> Requesting stop and notifying threads\n");
            stop_requested_ = true;
            batch_cv_.notify_all(); // Wake up any threads waiting for a batch
        }

        bool isStopRequested() const { return stop_requested_; }

        // Hook for message queue operations - called before/after message enqueue
        bool enqueueHook(size_t queue_length, MessageQueue::Message* message, bool is_pre_enqueue) const;

    protected:
        bool isShuttingDown() const { return stop_requested_; }

        int sendMessageBatch(
            std::shared_ptr<MessageQueue> msg_queue,
            std::shared_ptr<INetworkClient> network_client,
            uint32_t batch_count,
            char* batch_buf,
            uint32_t batch_buf_length) const;

        uint64_t next_wait_time_ms(uint64_t longest_wait_time_ms) const;
        bool waitForBatch(MessageQueue* first_queue, MessageQueue* second_queue) const;

        bool shouldSendBatch(std::shared_ptr<MessageQueue> queue) const {
            auto logger = LOG_THIS;
            if (!queue) return false;

            // Check queue length
            if (queue->length() >= max_batch_count_) {
                logger->debug2("SyslogSender::shouldSendBatch()> Queue length %zu exceeds max batch count %u\n",
                    queue->length(), max_batch_count_);
                return true;
            }

            // Check message age
            int64_t oldest_time = queue->getOldestMessageTimestamp();
            if (oldest_time > 0) {
                int64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                int64_t age = current_time - oldest_time;

                if (age >= max_batch_age_) {
                    logger->debug2("SyslogSender::shouldSendBatch()> Oldest message age %lld ms >= max age %llu ms\n",
                        age, max_batch_age_);
                    return true;
                }
            }

            return false;
        }

    private:
        std::atomic<bool> stop_requested_;

        const uint32_t max_batch_count_;
        const uint32_t max_batch_age_;

        std::shared_ptr<MessageQueue> primary_queue_;
        std::shared_ptr<MessageQueue> secondary_queue_;
        std::shared_ptr<INetworkClient> primary_network_client_;
        std::shared_ptr<INetworkClient> secondary_network_client_;
        std::shared_ptr<MessageBatcher> primary_batcher_;
        std::shared_ptr<MessageBatcher> secondary_batcher_;

        mutable std::mutex batch_mutex_;
        mutable std::condition_variable batch_cv_;
        std::unique_ptr<char[]> send_buffer_;
    };

} // namespace Syslog_agent