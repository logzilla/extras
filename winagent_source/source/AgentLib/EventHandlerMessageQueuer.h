#pragma once
#pragma once

#include <memory>
#include <string>
#include <map>
#include <sstream>
#include <vector>
#include "IEventHandler.h"
#include "EventLogEvent.h"
#include "Configuration.h"
#include "MessageQueue.h"
#include "../Infrastructure/Logger.h"
#include "SyslogAgentSharedConstants.h"
#include "windows.h"
#include "include/AgentLibExports.h"

using std::shared_ptr;
using std::map;
using std::vector;

namespace Syslog_agent {

    class AGENTLIB_API EventHandlerMessageQueuer : public IEventHandler {
    public:
        struct EventData {
            static constexpr size_t MAX_PROVIDER_LEN = 256;
            static constexpr size_t MAX_EVENT_ID_LEN = 32;
            static constexpr size_t MAX_MESSAGE_LEN = 32768;
            static constexpr size_t MAX_TIMESTAMP_LEN = 32;
            static constexpr size_t MAX_MICROSEC_LEN = 8;
            static constexpr size_t MAX_EVENT_DATA_PAIRS = 50;  // Maximum number of event data key-value pairs
            static constexpr size_t MAX_DOMAIN_NAME_LEN = 256;
            static constexpr size_t MAX_USER_NAME_LEN = 256;
            static constexpr size_t MAX_COMPUTER_LEN = 256;

            // All buffers are fixed-size to avoid heap allocations
            char provider[MAX_PROVIDER_LEN];
            char event_id[MAX_EVENT_ID_LEN];
            char message[MAX_MESSAGE_LEN];
            char timestamp[MAX_TIMESTAMP_LEN];
            char microsec[MAX_MICROSEC_LEN];
            unsigned char severity;
            char domain[MAX_DOMAIN_NAME_LEN];
            char user_name[MAX_USER_NAME_LEN];
            char computer[MAX_COMPUTER_LEN];

            // Structure for event data key-value pairs
            struct EventDataPair {
                char key[256];    // Fixed size for key
                char value[1024]; // Fixed size for value
                bool used;        // Whether this slot is used
            };
            EventDataPair event_data[MAX_EVENT_DATA_PAIRS];
            size_t event_data_count;  // Number of used pairs

            // Constructor to initialize arrays
            EventData() : severity(0), event_data_count(0) {
                provider[0] = '\0';
                event_id[0] = '\0';
                message[0] = '\0';
                timestamp[0] = '\0';
                microsec[0] = '\0';
                computer[0] = '\0';
                for (size_t i = 0; i < MAX_EVENT_DATA_PAIRS; i++) {
                    event_data[i].used = false;
                    event_data[i].key[0] = '\0';
                    event_data[i].value[0] = '\0';
                }
            }

            void parseFrom(EventLogEvent& event, const Configuration& config);

            // Helper function to safely copy strings
            static void safeCopyString(char* dest, size_t destSize, const char* src) {
                if (!src) {
                    dest[0] = '\0';
                    return;
                }
                size_t i;
                for (i = 0; i < destSize - 1 && src[i]; i++) {
                    dest[i] = src[i];
                }
                dest[i] = '\0';
            }

            // Helper to add event data pair
            bool addEventData(const char* key, const char* value) {
                if (event_data_count >= MAX_EVENT_DATA_PAIRS) return false;

                // Find unused slot
                for (size_t i = 0; i < MAX_EVENT_DATA_PAIRS; i++) {
                    if (!event_data[i].used) {
                        safeCopyString(event_data[i].key, sizeof(event_data[i].key), key);
                        safeCopyString(event_data[i].value, sizeof(event_data[i].value), value);
                        event_data[i].used = true;
                        event_data_count++;
                        return true;
                    }
                }
                return false;
            }
        };

        EventHandlerMessageQueuer(
            Configuration& configuration,
            shared_ptr<MessageQueue> primary_message_queue,
            shared_ptr<MessageQueue> secondary_message_queue,
            const wchar_t* log_name);

        virtual ~EventHandlerMessageQueuer() = default;

        Result handleEvent(const wchar_t* subscription_name, EventLogEvent& event) override;

    private:
        static constexpr double BUFFER_WARNING_THRESHOLD = 0.90;  // 90% as decimal
        static constexpr uint32_t ESTIMATED_FIELD_OVERHEAD = 20;  // Estimated overhead per field in bytes
        bool skipping_dates_ = false;

    protected:
        // Estimates final message size before generation
        size_t estimateMessageSize(const EventData& data, int logformat) const;
        Result generateLogMessage(EventLogEvent& event, const int logformat, char* json_buffer, size_t buflen);
        bool generateJson(const EventData& data, int logformat, char* json_buffer, size_t buflen);
        static unsigned char unixSeverityFromWindowsSeverity(char windows_severity_num);

        Configuration& configuration_;
        shared_ptr<MessageQueue> primary_message_queue_;
        shared_ptr<MessageQueue> secondary_message_queue_;
        string log_name_utf8_;
        string suffix_utf8_;
        uint32_t generated_count_ = 0;
    };

} // namespace Syslog_agent