#include "pch.h"

/*
SyslogAgent: a syslog agent for Windows
Copyright 2021 Logzilla Corp.
*/

//#include "../Agent/stdafx.h" // Removed conflicting PCH include
#include <ctime>
#include <iomanip>
#include <locale>
#include <sstream>
#include <iostream>
#include "SyslogAgentSharedConstants.h"
#include "EventHandlerMessageQueuer.h"
#include "../../Infrastructure/MessageBufferGuard.h"
#include "../Infrastructure/Logger.h"
#include "Globals.h"
#include "../Infrastructure/Util.h"
#include "../Infrastructure/OStreamBuf.h"
#include "SyslogSender.h"
#if 0
#include "StatefulLogger.h"
#endif

#include "../Infrastructure/XMLToJsonConverter.h"

#if ONLY_FOR_DEBUGGING_CURRENTLY_DISABLED
#include "EventLogger.h"
#endif

#include <stdio.h>

using namespace std;
using namespace Syslog_agent;

namespace {
    // RAII guard for automatically releasing message buffers from Globals
    // class MessageBufferGuard {
    //     ...
    // } // Removed local MessageBufferGuard
} // anonymous namespace

namespace Syslog_agent {

    void EventHandlerMessageQueuer::EventData::parseFrom(EventLogEvent& event, const Configuration& config) {
        auto logger = LOG_THIS;

        // Initialize user domain and name fields to empty
        domain[0] = '\0';
        user_name[0] = '\0';

        // Get provider
        safeCopyString(provider, MAX_PROVIDER_LEN, event.event_xml_data_.providerName);

        // Get computer field from the event XML
        safeCopyString(computer, MAX_COMPUTER_LEN, event.event_xml_data_.computer);

        // Get event ID
        safeCopyString(event_id, MAX_EVENT_ID_LEN, event.event_xml_data_.eventID);

        // Get message text
        auto message_value = event.getEventText();
        safeCopyString(message, MAX_MESSAGE_LEN, message_value);
        if (!message[0]) {
            safeCopyString(message, MAX_MESSAGE_LEN, "(no event message given)");
        }

        // Extract User SID if available
        domain[0] = '\0';
        user_name[0] = '\0';
        const char* user_sid = event.event_xml_data_.userID;

        if (user_sid && user_sid[0]) {
            if (config.getLookupAccounts()) {
                // User SID found, attempt to look it up
                printf("[DEBUG] EventData::parseFrom: About to call Globals::LookupUserSid for SID: %s\n", user_sid);
                fflush(stdout); // Ensure output is flushed immediately before the potentially crashing call

                DWORD result = Globals::instance()->LookupUserSid(
                    user_sid,
                    domain,
                    MAX_DOMAIN_NAME_LEN,
                    user_name,
                    MAX_USER_NAME_LEN
                );
                printf("[DEBUG] EventData::parseFrom: Globals::LookupUserSid returned %lu\n", result);
                fflush(stdout);

                if (result != ERROR_SUCCESS) {
                    // Lookup failed, just store the SID as the username
                    safeCopyString(user_name, MAX_USER_NAME_LEN, user_sid);
                }
            } else {
                // Account lookup disabled, just store the SID as the username
                safeCopyString(user_name, MAX_USER_NAME_LEN, user_sid);
            }
        }

        // Get and process timestamp from the event XML.
        // Expected format: "YYYY-MM-DDTHH:MM:SS[.microsecs]Z"
        const char* time_field = event.event_xml_data_.systemTime;

        int year, month, day, hour, minute, second;
        int microsecs = 0;
        // Parse the timestamp string. We expect at least 6 fields (microsecs is optional).
        int parsed = sscanf_s(time_field, "%d-%d-%dT%d:%d:%d.%dZ",
            &year, &month, &day, &hour, &minute, &second, &microsecs);
        if (parsed < 6) {
            // Log a recoverable error if the timestamp does not parse correctly.
            logger->recoverable_error("EventData::parseFrom(): Failed to parse timestamp \"%s\". Expected format \"YYYY-MM-DDTHH:MM:SS[.microsecs]Z\".\n", time_field);
            // Fallback: set timestamp to current time.
            std::time_t now = std::time(nullptr);
            snprintf(timestamp, MAX_TIMESTAMP_LEN, "%ld", static_cast<long>(now));
            snprintf(microsec, MAX_MICROSEC_LEN, "%06d", 0);
        }
        else {
            // Build a struct tm using the parsed values.
            struct tm tm = {};
            tm.tm_year = year - 1900;
            tm.tm_mon = month - 1;
            tm.tm_mday = day;
            tm.tm_hour = hour;
            tm.tm_min = minute;
            tm.tm_sec = second;
            // Convert to time_t (local time) then adjust for UTC offset.
            std::time_t event_time = mktime(&tm);
            // Apply UTC offset (cast to time_t to prevent potential overflow before subtraction)
            event_time -= static_cast<std::time_t>(config.getUtcOffsetMinutes()) * 60;
            snprintf(timestamp, MAX_TIMESTAMP_LEN, "%ld", static_cast<long>(event_time));
            snprintf(microsec, MAX_MICROSEC_LEN, "%06d", microsecs);
        }

        // Determine severity.
        if (config.getSeverity() == SharedConstants::Severities::DYNAMIC) {
            const char* level = event.event_xml_data_.level;
            char level_char = '\0';
            if (level && level[0]) {
                level_char = level[0];
            }
            severity = level_char ? unixSeverityFromWindowsSeverity(level_char)
                : SharedConstants::Severities::NOTICE;
        }
        else {
            severity = static_cast<unsigned char>(config.getSeverity());
        }

        // Parse additional event data fields.
        event_data_count = 0;

        // Process all <Data> elements from EventXmlData
        for (int i = 0; i < event.event_xml_data_.dataCount; i++) {
            const char* data_name = event.event_xml_data_.data[i].name;
            const char* data_value = event.event_xml_data_.data[i].value;

            if (data_name && data_name[0]) {
                addEventData(data_name, data_value);
            }
            else {
                // Handle unnamed data items with index
                char index_name[16];
                snprintf(index_name, sizeof(index_name), "Data%d", i);
                addEventData(index_name, data_value);
            }
        }
    }

    size_t EventHandlerMessageQueuer::estimateMessageSize(
        const EventHandlerMessageQueuer::EventData& data, int logformat) const
    {
        size_t estimated_size = 0;

        // Base JSON structure
        estimated_size += 2;  // Opening and closing braces

        // Root level fields
        if (!this->configuration_.getHostName().empty()) {
            estimated_size += 10 + this->configuration_.getHostName().length();  // "host": ""
        }
        estimated_size += 12 + strlen(data.provider);  // "program": ""
        estimated_size += 12 + strlen(data.message);   // "message": ""

        // extra_fields object
        estimated_size += 20;  // ", "extra_fields": {"
        estimated_size += 50;  // Basic fields (severity, facility, etc)
        estimated_size += strlen(data.event_id) + log_name_utf8_.length() + 30;
        estimated_size += strlen(data.computer) + 15; // computer field

        if (data.timestamp[0] != '\0') {
            estimated_size += strlen(data.timestamp) + strlen(data.microsec) + 25;
        }

        // Event data fields
        for (size_t i = 0; i < data.event_data_count; i++) {
            if (data.event_data[i].used) {
                estimated_size += strlen(data.event_data[i].key) + strlen(data.event_data[i].value) + ESTIMATED_FIELD_OVERHEAD;
            }
        }

        // Suffix if present
        if (!configuration_.getSuffix().empty()) {
            estimated_size += suffix_utf8_.length();
        }

        return estimated_size;
    }

    void epoch_to_datetime(std::time_t epoch, char* buffer, size_t bufsize) {
        if (bufsize < 20) return;  // Need 19 chars + null terminator

        // Convert to time struct
        std::tm timeinfo = {}; // Declare tm struct locally
        errno_t err = localtime_s(&timeinfo, &epoch); // Use localtime_s
        if (err != 0) {
            // Handle error, maybe log it
            return; // Or throw an exception
        }

        // Format into buffer
        std::strftime(buffer, bufsize, "%Y-%m-%d %H:%M:%S", &timeinfo); // Use the local struct
    }

    Result EventHandlerMessageQueuer::generateLogMessage(
        EventLogEvent& event, const int logformat, char* json_buffer, size_t buflen)
    {
        auto logger = LOG_THIS;
        logger->debug3("generateLogMessage entry, buflen=%zu\n", buflen);
        
        EventHandlerMessageQueuer::EventData data;
        data.parseFrom(event, this->configuration_);

        char* end;
        long event_timestamp_value = std::strtol(data.timestamp, &end, 10);

        if (*end == '\0') {
            // Successful conversion
            // Get seconds since epoch
            std::time_t now = std::time(nullptr);
            // Calculate earliest allowed timestamp using wider arithmetic to prevent overflow
            long long seconds_to_subtract = static_cast<long long>(SharedConstants::MAX_CATCHUP_DAYS) * 24 * 60 * 60;
            long earliest_allowed_timestamp = now - seconds_to_subtract;
            char buffer[20];  // YYYY-MM-DD HH:MM:SS\0
            epoch_to_datetime(event_timestamp_value, buffer, sizeof(buffer));
            if (event_timestamp_value < earliest_allowed_timestamp) {
                if (!skipping_dates_) {
                    skipping_dates_ = true;
                    logger->warning("Skipping events starting from %s\n", buffer);
                }
                logger->debug3("generateLogMessage skipping old event\n");
                return Result(ERROR_CANCELLED, "generateLogMessage", "Event too old, skipped.");
            }
            else {
                if (skipping_dates_) {
                    skipping_dates_ = false;
                    logger->info("End skipping dates starting at %s\n", buffer);
                }
            }
        }

        if (this->generateJson(data, logformat, json_buffer, buflen)) {
            size_t buf_len = strlen(json_buffer);
            logger->debug3("generateLogMessage - generateJson succeeded\n");
            logger->debug3("generateLogMessage - json_buffer len=%zu\n", buf_len);
            if (buf_len > 0) {
                logger->debug3("generateLogMessage - First 60 chars: [%.60s]\n", json_buffer);
            } else {
                logger->warning("generateLogMessage - Empty buffer after generateJson\n");
            }
            return Result(ERROR_SUCCESS, "generateLogMessage", "Successfully generated JSON message");
        }
        else {
            logger->warning("generateLogMessage - generateJson failed\n");
            return Result(ERROR_INVALID_DATA, "generateLogMessage", "Failed to generate JSON message");
        }
    }

    bool EventHandlerMessageQueuer::generateJson(
        const EventHandlerMessageQueuer::EventData& data, int logformat, char* json_buffer,
        size_t buflen)
    {
        auto logger = LOG_THIS;
        logger->debug3("generateJson entry, buflen=%zu\n", buflen);
        
        // Make sure buffer starts empty
        if (buflen > 0) {
            json_buffer[0] = '\0';
        }
        
        // Use stack-based buffer for stream
        OStreamBuf<char> ostream_buffer(json_buffer, buflen);
        std::ostream json_output(&ostream_buffer);

        auto checkBufferSpace = [&](const char* field_name, size_t needed_space) -> bool {
            std::streamoff current_len = ostream_buffer.current_length();
            if (static_cast<size_t>(current_len) + needed_space >= buflen) {
                logger->warning("Buffer overflow prevented: current %zu + needed %zu would exceed buffer size %zu while adding %s\n",
                    static_cast<size_t>(current_len), needed_space, buflen, field_name);
                logger->debug3("Buffer overflow prevented adding %s\n", field_name);
                return false;
            }
            return true;
            };

        // Start JSON object
        json_output << "{";
        logger->debug3("Started JSON object\
");

        // Add root level fields
        const string& hostname = configuration_.getHostName();
        logger->debug3("hostname from config: [%s]\n", hostname.c_str());

        // http ingestion will accept these at root
        if (!hostname.empty()) {
            if (!checkBufferSpace("hostname", hostname.length() + 10)) {
                return false;
            }
            json_output << "\"host\":\"" << hostname << "\",";
            logger->debug3("Added hostname\n");
        }

        // Add program and timestamp for HTTP format
        if (!checkBufferSpace("program", strlen(data.provider) + 20)) {
            return false;
        }
        json_output << "\"program\":\"" << (strlen(data.provider) > 0 ? data.provider : "missing") << "\"";

        json_output << ", ";
        // Start extra_fields for HTTP format
        if (logformat == SharedConstants::LOGFORMAT_HTTPPORT) {
            json_output << "\"extra_fields\": {";
            json_output << "\"host\":\"" << hostname << "\",";
            json_output << "\"program\":\"" << (strlen(data.provider) > 0 ? data.provider : "missing") << "\",";
            json_output << "\"computer\":\"" << data.computer << "\",";
        }
        json_output << "\"_source_type\": \"WindowsAgent\""
            << ", \"_source_tag\":\"windows_agent\""
            << ", \"_log_type\":\"eventlog\""
            << ", \"event_id\":\"" << data.event_id << "\""
            << ", \"event_log\":\"" << log_name_utf8_ << "\"";
        json_output << ", \"severity\":\"" << static_cast<unsigned int>(data.severity) << "\""
            << ", \"facility\":\"" << configuration_.getFacility() << "\"";

        // Include user domain and name information if available
        if (data.user_name[0] != '\0') {
            if (!checkBufferSpace("event_user_name", strlen(data.user_name) + 20)) {
                return false;
            }
            json_output << ", \"event_user_name\":\"" << data.user_name << "\"";

            if (data.domain[0] != '\0') {
                if (!checkBufferSpace("event_user_domain", strlen(data.domain) + 20)) {
                    return false;
                }
                json_output << ", \"event_user_domain\":\"" << data.domain << "\"";
            }
        }

        if (data.timestamp[0] != '\0') {
            if (!checkBufferSpace("timestamp", strlen(data.timestamp) + strlen(data.microsec) + 40)) {
                return false;
            }
            json_output << ", \"ts\": \"" << data.timestamp << "." << data.microsec << "\"";
        }

        // Add event data fields
        if (data.event_data_count > 0) {
            for (size_t i = 0; i < data.event_data_count; i++) {
                if (!data.event_data[i].used) continue;

                const size_t field_size = strlen(data.event_data[i].key) + strlen(data.event_data[i].value) + ESTIMATED_FIELD_OVERHEAD;
                if (!checkBufferSpace(data.event_data[i].key, field_size)) {
                    break;
                }

                MessageBufferGuard escaped_name_guard("jsonEscapeName", *Globals::instance());
                char* escaped_name = escaped_name_guard.get();
                MessageBufferGuard escaped_value_guard("jsonEscapeValue", *Globals::instance());
                char* escaped_value = escaped_value_guard.get();

                Util::jsonEscapeString(data.event_data[i].key, escaped_name, Globals::MESSAGE_BUFFER_SIZE);
                Util::jsonEscapeString(data.event_data[i].value, escaped_value, Globals::MESSAGE_BUFFER_SIZE);

                json_output << ", \"" << escaped_name << "\":\"" << escaped_value << "\"";
            }
        }

        // Add custom_suffix key-values to extra_fields if present
        if (!suffix_utf8_.empty()) {
            logger->debug3("generateJson: Adding suffix_utf8_: '%s' (length: %zu)\n", suffix_utf8_.c_str(), suffix_utf8_.length());
            json_output << ", " << suffix_utf8_;  // Already in "key":"value" format
        } else {
            logger->debug3("generateJson: suffix_utf8_ is empty\n");
        }

        size_t msg_len = strlen(data.message);

        MessageBufferGuard msg_buf_scoped("jsonEscapeMessage", *Globals::instance());
        if (!msg_buf_scoped.get()) {
            logger->recoverable_error("Failed to acquire msg_buf from Globals for jsonEscapeMessage\n");
            // Cannot proceed without msg_buf for escaping, though final_escaped_message will default to ""
            // Depending on strictness, could return false here.
            // For now, let it try to form JSON with an empty message if this happens.
        }
        char* msg_buf = msg_buf_scoped.get(); // Will be null if acquisition failed

        const char* final_escaped_message = ""; // Default to empty string

        MessageBufferGuard temp_buf_scoped("tempMessage", *Globals::instance());
        if (!temp_buf_scoped.get()) {
            logger->recoverable_error("Failed to acquire temp_buf from Globals for tempMessage\n");
            // Truncation might not be possible if this fails.
        }
        // char* temp_buf will be temp_buf_scoped.get(), used conditionally later

        size_t current_pos = static_cast<size_t>(ostream_buffer.pubseekoff(0, ios_base::cur));
        size_t remaining_space = buflen - current_pos;
        
        // Estimate overhead: ", \"message\":\"\"" + closing }/}} + null terminator
        size_t overhead = strlen(", \"message\":\"\"") + (logformat == SharedConstants::LOGFORMAT_HTTPPORT ? 2 : 1) + 1; 

        if (remaining_space <= overhead) {
            logger->recoverable_error("No space left for message field - buffer position %zu/%zu\n",
                current_pos, buflen);
            // Set message to empty, but continue to close JSON structure properly
        }
        else {
            remaining_space -= overhead; // Space available for actual escaped message content
            
            if (msg_len > 0) { // Only process if there is a message
                // Estimate escaped length (simple approximation)
                size_t estimated_escaped_len = msg_len * 2 + 10; 

                if (estimated_escaped_len <= remaining_space) {
                    // Fits without truncation (likely)
                    if (msg_buf) { // Ensure msg_buf itself is valid
                        Util::jsonEscapeString(data.message, msg_buf, Globals::MESSAGE_BUFFER_SIZE);
                        final_escaped_message = msg_buf;
                    } else {
                        logger->recoverable_error("Cannot perform full message escape; msg_buf is null\n");
                        // final_escaped_message remains ""
                    }
                } else {
                    // Needs truncation
                    const char* truncation_suffix = " *(message truncated)*";
                    size_t suffix_len = strlen(truncation_suffix);
                    // Rough estimate for max original chars before escaping
                    size_t max_msg_len = remaining_space > suffix_len ? (remaining_space - suffix_len) / 2 : 0;

                    if (max_msg_len > 0 && temp_buf_scoped.get()) {
                        char* temp_buf_ptr = temp_buf_scoped.get();
                        strncpy_s(temp_buf_ptr, Globals::MESSAGE_BUFFER_SIZE, data.message, max_msg_len);
                        temp_buf_ptr[max_msg_len] = '\0'; // Ensure null termination before strcat
                        strcat_s(temp_buf_ptr, Globals::MESSAGE_BUFFER_SIZE, truncation_suffix);

                        if (msg_buf) { // Ensure msg_buf itself is valid before writing to it
                           Util::jsonEscapeString(temp_buf_ptr, msg_buf, Globals::MESSAGE_BUFFER_SIZE);
                           final_escaped_message = msg_buf;
                        } else {
                           // msg_buf acquisition failed earlier, cannot escape. Message remains "".
                           logger->recoverable_error("Cannot perform truncation escape; msg_buf is null\n");
                        }
                        logger->warning("Message truncated from %zu to roughly %zu original chars due to buffer limit %zu\n", msg_len, max_msg_len, buflen);
                    } else if (max_msg_len > 0 && !temp_buf_scoped.get()) {
                        logger->recoverable_error("Cannot truncate message; temp_buf acquisition failed.\n");
                        // Attempt to use non-truncated if it fits, otherwise message becomes empty
                        if (msg_len * 2 + 10 <= remaining_space && msg_buf) { // Re-check original condition if temp_buf failed
                            Util::jsonEscapeString(data.message, msg_buf, Globals::MESSAGE_BUFFER_SIZE);
                            final_escaped_message = msg_buf;
                        } else {
                            logger->recoverable_error("Cannot use original message either after temp_buf failure.\n");
                        }
                    } else {
                         logger->recoverable_error("No space left for truncated message content - buffer position %zu/%zu\n",
                            current_pos, buflen);
                        // final_escaped_message remains ""
                    }
                }
            }
        }

        json_output << ", \"message\":\"" << final_escaped_message << "\"";

        // temp_buf is now managed by temp_buf_scoped, no explicit release needed here.
        // Close extra_fields if necessary and add message at root level for HTTP format
        if (logformat == SharedConstants::LOGFORMAT_HTTPPORT) {
            // now put message outside extra_fields as well...
            // i know, this sucks, it's just the way the lz appstore app is written
            if (!checkBufferSpace("closing extra_fields and adding root message", strlen(final_escaped_message) + 20)) return false;
            json_output << "}, \"message\":\"" << final_escaped_message << "\""; // Close extra_fields and add message at root
        }
        else {
            // Just close extra_fields for non-HTTP format
            if (!checkBufferSpace("closing extra_fields", 1)) return false;
            json_output << "}"; // Close extra_fields
        }

        // Close the main JSON object
        if (!checkBufferSpace("closing main object", 2)) return false; // Check space for '}' and null terminator
        json_output << "}" << std::ends;

        // msg_buf is now managed by msg_buf_scoped, no explicit release needed here.

        // Check stream state AFTER writing everything
        if (json_output.fail()) {
             logger->recoverable_error("JSON output stream failed during generation. Buffer position %zu/%zu\n",
                 static_cast<size_t>(ostream_buffer.pubseekoff(0, ios_base::cur)), buflen);
             // Ensure buffer is null-terminated even on failure, if possible
             if (buflen > 0) json_buffer[0] = '\0'; 
             return false;
        }

        // === DEBUGGING START ===
        size_t final_len = strlen(json_buffer);
        logger->debug3("generateJson: final_len = %zu, buffer start: [%.100s]\n", final_len, json_buffer);
        // === DEBUGGING END ===

        return true;
    }

    EventHandlerMessageQueuer::EventHandlerMessageQueuer(
        Configuration& configuration,
        shared_ptr<MessageQueue> primary_message_queue,
        shared_ptr<MessageQueue> secondary_message_queue,
        const wchar_t* log_name)
        : configuration_(configuration),
        primary_message_queue_(primary_message_queue),
        secondary_message_queue_(secondary_message_queue)
    {
        auto logger = LOG_THIS;
        DWORD chars_written;
        size_t length = wcslen(log_name);
        // Safely compare size_t with INT_MAX by casting
        if (length > static_cast<size_t>(INT_MAX)) {
            throw std::runtime_error("Log name too long");
        }

        log_name_utf8_.resize(SharedConstants::MAX_LOG_NAME_LENGTH + 1);
        chars_written = WideCharToMultiByte(CP_UTF8, 0, log_name,
            static_cast<int>(length), log_name_utf8_.data(), log_name_utf8_.size() - 1,
            NULL, NULL);

        if (chars_written == 0) {
            throw std::runtime_error("Failed to convert log name to UTF-8");
        }
        log_name_utf8_.resize(chars_written);

        if (!configuration_.getSuffix().empty()) {
            auto suffix = configuration_.getSuffix();
            logger->debug2("Configuration has suffix: '%ls'\n", suffix.c_str());
            
            // Safely compare suffix length (size_t) with MAX_SUFFIX_LENGTH (likely int)
            if (suffix.length() > static_cast<size_t>(SharedConstants::MAX_SUFFIX_LENGTH) - 1) {
                suffix_utf8_ = string("\"error_suffix\": \"too long\"");
                logger->warning("Suffix too long, using error placeholder\n");
            }
            else {
                // Use a safer stack-based temporary buffer approach
                char temp_buffer[SharedConstants::MAX_SUFFIX_LENGTH + 1];
                memset(temp_buffer, 0, sizeof(temp_buffer)); // Ensure buffer is clean
                
                chars_written = Util::wstr2str_truncate(temp_buffer, sizeof(temp_buffer), suffix.c_str());
                logger->debug3("wstr2str_truncate returned %zu chars, buffer contains: '%s'\n", chars_written, temp_buffer);
                
                if (chars_written == 0) {
                    suffix_utf8_ = string("\"error_suffix\": \"conversion failed\"");
                    logger->warning("Suffix conversion failed, using error placeholder\n");
                }
                else {
                    // Safely copy the converted string to suffix_utf8_
                    suffix_utf8_ = string(temp_buffer, chars_written);
                    logger->debug3("Suffix converted to UTF-8: '%s' (length: %zu)\n", suffix_utf8_.c_str(), suffix_utf8_.length());
                }
            }
        }
    }

    Result EventHandlerMessageQueuer::handleEvent(
        const wchar_t* subscription_name, EventLogEvent& event)
    {
        auto logger = LOG_THIS;
        // logger->debug3("handleEvent entry\n");

        char* json_buffer = Globals::instance()->getMessageBuffer("eventHandlerMessageQueuer");
        
        // Ensure buffer starts empty
        if (json_buffer) {
            json_buffer[0] = '\0';
            // logger->debug3("handleEvent - zeroed buffer\n");
        } else {
            logger->recoverable_error("handleEvent - Failed to get buffer\n");
        }

        try {
            // Estimate message size and check buffer capacity
            EventData data;
            event.renderEvent();
            // logger->debug3("handleEvent - called renderEvent()\
");
            
            data.parseFrom(event, configuration_);
            // logger->debug3("handleEvent - parsed data\n");
            
            bool event_id_filter_match = configuration_.getEventIdFilter().count(event.getEventId()) > 0;
            if (configuration_.getIncludeVsIgnoreEventIds() && !event_id_filter_match) {
                return Result(ERROR_CANCELLED, "handleEvent", "Event ID not in filter list, skipped.");
            }
            else if (!configuration_.getIncludeVsIgnoreEventIds() && event_id_filter_match) {
                return Result(ERROR_CANCELLED, "handleEvent", "Event ID in filter list, skipped.");
            }

            size_t estimated_size = estimateMessageSize(data, SharedConstants::LOGFORMAT_HTTPPORT);
            // logger->debug3("handleEvent - estimated size: %zu\n", estimated_size);

            if (estimated_size > Globals::MESSAGE_BUFFER_SIZE) {
                logger->recoverable_error("Estimated message size %zu exceeds buffer size %zu\n",
                    estimated_size, Globals::MESSAGE_BUFFER_SIZE);
                Globals::instance()->releaseMessageBuffer(json_buffer);
                return Result(ERROR_INSUFFICIENT_BUFFER, "handleEvent", "Buffer too small");
            }

            // Generate JSON for primary queue
            Result generate_result = generateLogMessage(event, configuration_.getPrimaryLogformat(), json_buffer, Globals::MESSAGE_BUFFER_SIZE);
            // logger->debug3("handleEvent - generateLogMessage returned status: %d\n", generate_result.statusCode());            
            if (generate_result.statusCode() != ERROR_SUCCESS) {
                Globals::instance()->releaseMessageBuffer(json_buffer);
                if (generate_result.statusCode() != ERROR_CANCELLED) {
                    logger->warning("Failed to generate JSON for primary queue (error %d)\n",
                        generate_result.statusCode());
                }
                return generate_result;
            }

            // Queue message for primary server
            size_t buffer_len = strlen(json_buffer);
            // logger->debug3("handleEvent - buffer length before enqueue: %zu\n",
            //     buffer_len);
            if (buffer_len > 0) {
                // logger->debug3("handleEvent - First 60 chars: [%.60s]\n",
                //     json_buffer);
            } else {
                logger->warning("handleEvent - Empty buffer before enqueue!\n");
                Globals::instance()->releaseMessageBuffer(json_buffer);
                return Result(ERROR_INVALID_DATA, "handleEvent", "Generated JSON message is empty");
            }
            
            // Attempt to enqueue and check result
            bool enqueue_result = primary_message_queue_->enqueue(json_buffer, buffer_len);
            if (!enqueue_result) {
                logger->verbose("Failed to enqueue message of length %zu to primary queue\n",
                    buffer_len);
                Globals::instance()->releaseMessageBuffer(json_buffer);
                return Result(ERROR_WRITE_FAULT, "handleEvent", "Failed to enqueue message to primary queue");
            }

            // Handle secondary server if configured
            if (configuration_.hasSecondaryHost()) {
                // Generate JSON for secondary queue
                generate_result = generateLogMessage(event, configuration_.getSecondaryLogformat(), json_buffer, Globals::MESSAGE_BUFFER_SIZE);
                if (generate_result.statusCode() != ERROR_SUCCESS) {
                    Globals::instance()->releaseMessageBuffer(json_buffer);
                    if (generate_result.statusCode() != ERROR_CANCELLED) {
                        logger->recoverable_error("Failed to generate JSON for secondary queue\n");
                    }
                    return generate_result;
                }

                // Queue message for secondary server
                secondary_message_queue_->enqueue(json_buffer, strlen(json_buffer));
            }

            Globals::instance()->releaseMessageBuffer(json_buffer);
            return Result();
        }
        catch (const std::exception& e) {
            logger->recoverable_error("Exception in handleEvent: %s\n", e.what());
            Globals::instance()->releaseMessageBuffer(json_buffer);
            return Result(ERROR_INVALID_DATA, "handleEvent", e.what());
        }
    }

    unsigned char EventHandlerMessageQueuer::unixSeverityFromWindowsSeverity(
        char windows_severity_num)
    {
        auto logger = LOG_THIS;
        unsigned char severity;

        switch (windows_severity_num) {
        case '0':
            severity = SharedConstants::Severities::ALERT;
            break;
        case '1':
            severity = SharedConstants::Severities::CRITICAL;
            break;
        case '2':
            severity = SharedConstants::Severities::ERR;
            break;
        case '3':
            severity = SharedConstants::Severities::WARNING;
            break;
        case '4':
            severity = SharedConstants::Severities::NOTICE;
            break;
        case '5':
            severity = SharedConstants::Severities::DEBUG;
            break;
        default:
            logger->warning("Unknown Windows severity level: %c, defaulting to NOTICE\n", windows_severity_num);
            severity = SharedConstants::Severities::NOTICE;
        }

        return severity;
    }

} // namespace Syslog_agent