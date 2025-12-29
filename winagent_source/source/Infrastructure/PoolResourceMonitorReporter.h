#pragma once

#include "PoolResourceMonitor.h"
#include "Logger.h"
#include <chrono>
#include <atomic>

/**
 * Helper class to periodically report memory pool statistics
 * to detect potential memory leaks over time.
 */
class PoolResourceMonitorReporter {
private:
    std::chrono::steady_clock::time_point last_report_time_;
    std::chrono::seconds report_interval_;
    std::atomic<bool> enabled_;

    // Private constructor for singleton
    PoolResourceMonitorReporter()
        : last_report_time_(std::chrono::steady_clock::now()),
          report_interval_(std::chrono::seconds(300)), // Default 5 minutes
          enabled_(true) {}

    // No copy constructor or assignment
    PoolResourceMonitorReporter(const PoolResourceMonitorReporter&) = delete;
    PoolResourceMonitorReporter& operator=(const PoolResourceMonitorReporter&) = delete;

public:
    /**
     * Get the singleton instance
     */
    static PoolResourceMonitorReporter& instance() {
        static PoolResourceMonitorReporter reporter_instance;
        return reporter_instance;
    }

    /**
     * Set the reporting interval
     * @param seconds Interval in seconds between reports
     */
    void setReportInterval(int seconds) {
        report_interval_ = std::chrono::seconds(seconds);
    }

    /**
     * Enable or disable automatic reporting
     */
    void setEnabled(bool enabled) {
        enabled_ = enabled;
    }

    /**
     * Check if it's time to report and log statistics if needed.
     * This should be called periodically from a safe context.
     */
    void checkAndReport() {
        if (!enabled_) {
            return;
        }

        auto now = std::chrono::steady_clock::now();
        if (now - last_report_time_ >= report_interval_) {
            reportNow();
            last_report_time_ = now;
        }
    }

    /**
     * Force an immediate report of memory statistics
     */
    void reportNow() {
        auto logger = LOG_THIS;
        
        logger->info("--- Memory Pool Resources Report ---");
        
        // Log overall statistics
        PoolResourceMonitor::instance().logStats();
        
        // Get and report potential leak suspects
        auto suspects = PoolResourceMonitor::instance().getLeakSuspects();
        if (!suspects.empty()) {
            logger->warning("Potential memory leaks detected!");
            for (const auto& suspect : suspects) {
                int64_t diff = static_cast<int64_t>(suspect.allocation_count) - 
                              static_cast<int64_t>(suspect.release_count);
                logger->warning("  Pool '%s': Potential leak of %lld objects (allocated %zu, released %zu)", 
                    suspect.name.c_str(), diff, suspect.allocation_count, suspect.release_count);
            }
        } else {
            logger->info("No potential memory leaks detected.");
        }
        
        logger->info("--- End Memory Pool Resources Report ---");
    }
};
