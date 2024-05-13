/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#include <limits>
#include "stdafx.h"
#include <stdio.h>
#include <ws2tcpip.h>
#include "Globals.h"
#include "Result.h"
#include "SyslogSender.h"
#include "Util.h"
#undef max // Undefine max to use std::numeric_limits<>::max()


using namespace Syslog_agent;

#define IDLE_INTERVAL 20000

WindowsTimer SyslogSender::enqueue_timer_;
volatile bool SyslogSender::stop_requested_ = false;
const char SyslogSender::message_header_[] = "{ \"events\": [ ";
const char SyslogSender::message_separator_[] = ", ";
const char SyslogSender::message_trailer_[] = " ] }";


SyslogSender::SyslogSender(
    Configuration& config,
    shared_ptr<MessageQueue> primary_queue,
    shared_ptr<MessageQueue> secondary_queue,
    shared_ptr<NetworkClient> primary_network_client,
    shared_ptr<NetworkClient> secondary_network_client
) :
    config_(config),
    primary_queue_(primary_queue),
    secondary_queue_(secondary_queue),
    primary_network_client_(primary_network_client),
    secondary_network_client_(secondary_network_client)
{
    message_buffer_ = make_unique<char[]>(MAX_MESSAGE_SIZE);
}

int SyslogSender::sendMessageBatch(
    shared_ptr<MessageQueue> msg_queue, shared_ptr<NetworkClient> network_client, 
    char* buf) const {
    int batch_count = 0;
    char* sep;
    int msg_size = -1;
    bool connected;
    bool posted;

    // NOTE: this must be run within the message queue lock

    memcpy(message_buffer_.get(), message_header_, strlen(message_header_));
    uint32_t message_buffer_length = static_cast<uint32_t>(strlen(message_header_));
    sep = "";
    while (batch_count < msg_queue->length()) {
        msg_size = msg_queue->peek(buf, Globals::MESSAGE_BUFFER_SIZE, batch_count);
        if (msg_size + message_buffer_length + strlen(sep)
            + strlen(message_trailer_) < MAX_MESSAGE_SIZE) {
            memcpy(message_buffer_.get() + message_buffer_length, sep, strlen(sep));
            message_buffer_length += static_cast<uint32_t>(strlen(sep));
            memcpy(message_buffer_.get() + message_buffer_length, buf, msg_size);
            message_buffer_length += msg_size;
            batch_count++;
        }
        else {
            break;
        }
        sep = const_cast<char*>(message_separator_);
    }
    memcpy(message_buffer_.get() + message_buffer_length, message_trailer_,
        strlen(message_trailer_));
    message_buffer_length += static_cast<uint32_t>(strlen(message_trailer_));
    Logger::debug2("SyslogSender::sendMessageBatch() sending msg %d bytes\n",
        message_buffer_length);

    connected = network_client->connect();
    if (!connected) {
        Logger::recoverable_error("SyslogSender::sendMessageBatch() server not"
            " connected, error: %u\n", GetLastError());
    }
    else {
        posted = network_client->post(message_buffer_.get(),
            message_buffer_length);
        if (posted == NetworkClient::RESULT_SUCCESS) {
            Logger::debug2("SyslogSender::sendMessageBatch() message sent to"
                " server (bytes %d)\n", msg_size);
            for (int i = 0; i < batch_count; i++) {
                msg_queue->removeFront();
            }
        }
    }
	return batch_count;
}


void SyslogSender::run() const {

    Logger::debug2("Syslog_sender::run() starting\n");
    char* buf = Globals::instance()->getMessageBuffer("SyslogSender::run()");
    int64_t last_start_loop_time = Util::getUnixTimeMilliseconds();

    while (!SyslogSender::stop_requested_) {

        int64_t current_time = Util::getUnixTimeMilliseconds();
        int64_t elapsed_time_64 = current_time - last_start_loop_time;  // Perform the subtraction once
        unsigned int elapsed_time;
        if (elapsed_time_64 < 0) {
            Logger::warn("SyslogSender::run() elapsed time < 0\n");
            elapsed_time = 0;
        }
        else {
            // Check the result before casting
            if (static_cast<uint64_t>(elapsed_time_64) > std::numeric_limits<unsigned int>::max()) {
                throw std::overflow_error("Resulting difference is out of range for unsigned int.");
            }
            elapsed_time = static_cast<int>(elapsed_time_64);
        }

        if (elapsed_time > SharedConstants::SENDER_MAINLOOP_DURATION) {
            elapsed_time = SharedConstants::SENDER_MAINLOOP_DURATION;
        }
        if (elapsed_time < SharedConstants::SENDER_MAINLOOP_DURATION) {
            Sleep(SharedConstants::SENDER_MAINLOOP_DURATION - elapsed_time);
        }

        last_start_loop_time = Util::getUnixTimeMilliseconds();

        string response;

        if (config_.primary_logzilla_version_
            == SharedConstants::Defaults::VERSION_DETECT_STR) {
            Logger::debug2("SyslogSender::run() detecting primary LogZilla version\n");
            string version = primary_network_client_->getLogzillaVersion();
            if (version == "") {
                Logger::recoverable_error("SyslogSender::run() need primary"
                    " LogZilla version and could not get, error: %u\n", GetLastError());
            }
            else {
                Logger::debug2("SyslogSender::run() primary server LogZilla"
                    " version: %s\n", version.c_str());
                config_.setPrimaryLogzillaVersion(version);
            }
        }

        if (config_.hasSecondaryHost() && config_.secondary_logzilla_version_
            == SharedConstants::Defaults::VERSION_DETECT_STR) {
            Logger::debug2("SyslogSender::run() detecting secondary LogZilla version\n");
            string version = secondary_network_client_->getLogzillaVersion();
            if (version == "") {
                Logger::recoverable_error("SyslogSender::run() need secondary"
                    " LogZilla version and could not get, error: %u\n", GetLastError());
            }
            else {
                Logger::debug2("SyslogSender::run() secondary server LogZilla"
                    " version: %s\n", version.c_str());
                config_.setSecondaryLogzillaVersion(version);
            }
        }

        if (!primary_queue_->isEmpty()) {

            Logger::debug2("Sending primary queue:\n");
            primary_queue_->runInsideLock([&]() -> int {
                return sendMessageBatch(primary_queue_, primary_network_client_, buf);
                });
            Logger::debug2("Syslog_sender::run() primary queue size after:"
                " sendMessageBatch: %d\n", primary_queue_->length());
        }

        if (secondary_queue_ != nullptr && !secondary_queue_->isEmpty()) {
            Logger::debug2("Sending secondary queue:\n");
            primary_queue_->runInsideLock([&]() -> int {
                return sendMessageBatch(secondary_queue_, secondary_network_client_, buf);
                });
            Logger::debug2("Syslog_sender::run() secondary queue size after:"
                " sendMessageBatch: %d\n", primary_queue_->length());
        }
    }
    Globals::instance()->releaseMessageBuffer("SyslogSender::run()", buf);
    Logger::debug2("Syslog_sender::run() ending\n");
}
