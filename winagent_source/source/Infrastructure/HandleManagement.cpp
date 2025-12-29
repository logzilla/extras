#include "pch.h"
#include "HandleManagement.h"
#include "Logger.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <vector>

namespace Infrastructure {

// Singleton instance for the handle tracker
static HandleTracker g_handleTracker;

// Definition of the getHandleTracker function
INFRASTRUCTURE_API HandleTracker& getHandleTracker() {
    return g_handleTracker;
}

void HandleTracker::reportActiveHandles() const {
    auto logger = LOG_THIS;
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (active_handles_.empty()) {
        logger->debug("HandleTracker: No active handles tracked");
        return;
    }
    
    // Copy to vector for sorting
    std::vector<std::pair<uintptr_t, HandleInfo>> handles;
    handles.reserve(active_handles_.size());
    
    for (const auto& pair : active_handles_) {
        handles.push_back(pair);
    }
    
    // Sort by type then by creation time
    std::sort(handles.begin(), handles.end(), 
        [](const auto& a, const auto& b) {
            if (a.second.type == b.second.type) {
                return a.second.creation_time < b.second.creation_time;
            }
            return a.second.type < b.second.type;
        });
    
    logger->debug("HandleTracker: %zu active handle(s) currently tracked:", handles.size());
    
    // Group by type for more readable output
    std::string currentType;
    for (const auto& pair : handles) {
        const auto& info = pair.second;
        
        // Print type header if changed
        if (currentType != info.type) {
            currentType = info.type;
            logger->debug("  Type: %s", currentType.c_str());
        }
        
        // Format creation time
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - info.creation_time).count();
        
        std::stringstream ageStr;
        if (age < 60) {
            ageStr << age << "s";
        } else if (age < 3600) {
            ageStr << (age / 60) << "m " << (age % 60) << "s";
        } else {
            ageStr << (age / 3600) << "h " << ((age % 3600) / 60) << "m";
        }
        
        logger->debug("    Handle: 0x%08X, Name: %s, Age: %s, Thread: %u, Created at: %s",
            static_cast<unsigned int>(pair.first),
            info.name.c_str(),
            ageStr.str().c_str(),
            info.thread_id,
            info.location.c_str());
    }
}

} // namespace Infrastructure 