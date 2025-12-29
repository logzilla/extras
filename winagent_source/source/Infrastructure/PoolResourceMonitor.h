#pragma once

#include "Logger.h"
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>


/**
    * Tracks pool resource allocations and releases to help identify potential leaks.
    */
class PoolResourceMonitor {
private:
    struct PoolStats {
        std::string name;
        size_t current_usage;
        size_t peak_usage;
        size_t allocation_count;
        size_t release_count;
            
        PoolStats()
            : name(), current_usage(0), peak_usage(0), 
                allocation_count(0), release_count(0) {}
    };
        
    mutable std::mutex stats_mutex_;
    std::unordered_map<std::string, PoolStats> pool_stats_;

    // Make constructor private for Singleton
    PoolResourceMonitor() = default;

public:
    // Delete copy constructor and assignment operator
    PoolResourceMonitor(const PoolResourceMonitor&) = delete;
    PoolResourceMonitor& operator=(const PoolResourceMonitor&) = delete;

    // Static method to get the single instance
    static PoolResourceMonitor& instance() {
        // Thread-safe initialization (Meyers' Singleton)
        static PoolResourceMonitor monitor_instance; 
        return monitor_instance;
    }

    void recordAllocation(const std::string& pool_name) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        auto& stats = pool_stats_[pool_name];
        stats.name = pool_name;
        stats.current_usage++;
        stats.allocation_count++;
        stats.peak_usage = (std::max)(stats.peak_usage, stats.current_usage);
    }
        
    void recordRelease(const std::string& pool_name) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        auto& stats = pool_stats_[pool_name];
        if (stats.current_usage > 0) {
            stats.current_usage--;
        }
        stats.release_count++;
    }
        
    size_t getAllocationCount(const std::string& pool_name) const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        auto it = pool_stats_.find(pool_name);
        if (it != pool_stats_.end()) {
            return it->second.allocation_count;
        }
        return 0;
    }
        
    size_t getReleaseCount(const std::string& pool_name) const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        auto it = pool_stats_.find(pool_name);
        if (it != pool_stats_.end()) {
            return it->second.release_count;
        }
        return 0;
    }
        
    std::vector<PoolStats> getLeakSuspects() const {
        std::vector<PoolStats> suspects;
        std::lock_guard<std::mutex> lock(stats_mutex_);
            
        for (const auto& pair : pool_stats_) {
            const auto& stats = pair.second;
            // Identify pools with allocation/release discrepancy
            if (stats.allocation_count > stats.release_count) {
                suspects.push_back(stats);
            }
        }
            
        // Sort by highest leak count
        std::sort(suspects.begin(), suspects.end(), 
            [](const PoolStats& a, const PoolStats& b) {
                return (a.allocation_count - a.release_count) > 
                        (b.allocation_count - b.release_count);
            });
                
        return suspects;
    }
        
    void logStats() const {
        auto logger = LOG_THIS;
        std::lock_guard<std::mutex> lock(stats_mutex_);
            
        logger->info("PoolResourceMonitor: Pool usage statistics:\n");
        for (const auto& pair : pool_stats_) {
            const auto& stats = pair.second;
            logger->info("  Pool '%s': current=%zu, peak=%zu, alloc=%zu, release=%zu, potential_leak=%zu\n",
                stats.name.c_str(),
                stats.current_usage,
                stats.peak_usage,
                stats.allocation_count,
                stats.release_count,
                stats.allocation_count - stats.release_count);
        }
    }
    
    void logMemoryHealth() const {
        auto logger = LOG_THIS;
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        // Check for potential memory leaks and other issues
        bool found_issues = false;
        
        // Check for significant allocation/release discrepancies
        for (const auto& pair : pool_stats_) {
            const auto& stats = pair.second;
            size_t leak_count = stats.allocation_count > stats.release_count ?
                (stats.allocation_count - stats.release_count) : 0;
            
            // If leak is more than 5% of total allocations and at least 5 objects, report it
            if (leak_count > 5 && stats.allocation_count > 0 &&
                ((leak_count * 100) / stats.allocation_count) > 5) {
                
                if (!found_issues) {
                    logger->warning("PoolResourceMonitor: Potential memory issues detected:\n");
                    found_issues = true;
                }
                
                logger->warning("  Pool '%s': Potential leak of %zu objects (%.1f%% of %zu allocations)\n",
                    stats.name.c_str(),
                    leak_count,
                    (leak_count * 100.0f) / stats.allocation_count,
                    stats.allocation_count);
            }
        }
        
        if (!found_issues) {
            logger->info("PoolResourceMonitor: No significant memory issues detected\n");
        }
    }
        
    void resetStats() {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        pool_stats_.clear();
    }
};
