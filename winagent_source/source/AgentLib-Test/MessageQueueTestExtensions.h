#pragma once

#include "../AgentLib/MessageQueue.h"

namespace Syslog_agent {
    // Extension for testing: Add a no-parameter dequeue method
    // Cannot use inline member function extension, so create a non-member function instead
    inline bool dequeue(MessageQueue* queue) {
        if (!queue) return false;
        char buffer[2048]; // Temporary buffer
        return queue->dequeue(buffer, sizeof(buffer)) > 0;
    }
} 