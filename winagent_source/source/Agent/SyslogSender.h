/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once

#include <memory>
#include "Configuration.h"
#include "MessageQueue.h"
#include "NetworkClient.h"
#include "WindowsTimer.h"

namespace Syslog_agent {

    class SyslogSender {
    public:
        SyslogSender(
            Configuration& config,
            shared_ptr<MessageQueue> primary_queue,
            shared_ptr<MessageQueue> secondary_queue,
            shared_ptr<NetworkClient> primary_network_client,
            shared_ptr<NetworkClient> secondary_network_client
        );
        static WindowsTimer enqueue_timer_; // TODO : this probably shouldn't be static
        void run() const;
        static void stop() { 
            Logger::log(Logger::DEBUG2, "SyslogSender::stop() stop requested\n");
            SyslogSender::stop_requested_ = true;
        }

    private:
        static constexpr int MAX_MESSAGE_SIZE = 65535;
        static const char message_header_[];
        static const char message_separator_[];
        static const char message_trailer_[];
        Configuration& config_;
        shared_ptr<MessageQueue> primary_queue_;
        shared_ptr<MessageQueue> secondary_queue_;
        shared_ptr<NetworkClient> primary_network_client_;
        shared_ptr<NetworkClient> secondary_network_client_;
        volatile static bool stop_requested_;
        unique_ptr<char[]> message_buffer_;

        int sendMessageBatch(shared_ptr<MessageQueue> msg_queue, shared_ptr
            <NetworkClient> network_client, char* buf) const;
    };
}
