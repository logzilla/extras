#pragma once

#include "../Infrastructure/PoolResourceMonitor.h"

namespace Syslog_agent {

/**
 * A wrapper class that safely accesses PoolResourceMonitor::instance()
 * with fallback behavior if the symbol isn't available at link time.
 */
class PoolResourceMonitorWrapper {
public:
    static void recordAllocation(const std::string& name) {
        #ifdef _DEBUG
        try {
            PoolResourceMonitor::instance().recordAllocation(name);
        }
        catch (...) {
            // Silently ignore if the resource monitor isn't available
        }
        #endif
    }
    
    static void recordRelease(const std::string& name) {
        #ifdef _DEBUG
        try {
            PoolResourceMonitor::instance().recordRelease(name);
        }
        catch (...) {
            // Silently ignore if the resource monitor isn't available
        }
        #endif
    }
    
    /**
     * Log the current statistics of all memory pools
     */
    static void logStats() {
        try {
            PoolResourceMonitor::instance().logStats();
        }
        catch (...) {
            // Silently ignore if the resource monitor isn't available
        }
    }
    
    /**
     * Check and log potential memory health issues
     */
    static void checkMemoryHealth() {
        try {
            PoolResourceMonitor::instance().logMemoryHealth();
        }
        catch (...) {
            // Silently ignore if the resource monitor isn't available
        }
    }
};

} // namespace Syslog_agent 