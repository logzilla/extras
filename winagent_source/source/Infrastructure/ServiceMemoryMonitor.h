/* Copyright 2025 Logzilla Corp. */

#pragma once
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <chrono>
#include <thread>
#include <atomic>
#include "ResourceMonitor.h"
#include "Logger.h"

/**
 * ServiceMemoryMonitor - A centralized class for monitoring memory usage across the service
 *
 * This class tracks memory usage periodically across multiple components, provides alerts when
 * usage exceeds thresholds, and can take corrective actions when memory use is excessive.
 */
class ServiceMemoryMonitor {
public:
    // Function signature for memory stats callback
    using MemoryStatsCallback = std::function<size_t()>;
    
    struct MemorySource {
        std::string name;
        MemoryStatsCallback callback;
        size_t lastValue;
        size_t warningThreshold;
        size_t criticalThreshold;
        bool enabled;
    };

    /**
     * Constructor
     * 
     * @param pollIntervalMs Milliseconds between polling for memory stats
     */
    ServiceMemoryMonitor(int pollIntervalMs = 30000);
    
    /**
     * Destructor - stops monitoring and cleans up resources
     */
    ~ServiceMemoryMonitor();
    
    /**
     * Register a memory source to monitor
     * 
     * @param name Descriptive name for the memory source
     * @param callback Function that returns current memory usage count
     * @param warningThreshold Count at which to log a warning
     * @param criticalThreshold Count at which to log a critical error
     * @return ID for the registered source
     */
    int registerMemorySource(
        const std::string& name,
        MemoryStatsCallback callback,
        size_t warningThreshold,
        size_t criticalThreshold
    );
    
    /**
     * Unregister a memory source
     * 
     * @param sourceId ID of the source to remove
     */
    void unregisterMemorySource(int sourceId);
    
    /**
     * Start periodic monitoring
     */
    void startMonitoring();
    
    /**
     * Stop periodic monitoring
     */
    void stopMonitoring();
    
    /**
     * Check memory usage immediately (doesn't affect periodic monitoring)
     * 
     * @param logAll If true, logs all memory sources regardless of thresholds
     */
    void checkMemoryUsage(bool logAll = false);
    
    /**
     * Register a callback to be executed when memory usage exceeds critical thresholds
     * 
     * @param callback Function to call when memory is critical
     */
    void registerCriticalMemoryCallback(std::function<void()> callback);
    
    /**
     * Get singleton instance
     */
    static ServiceMemoryMonitor& instance();
    
private:
    void monitoringThreadFunc();
    
    int pollIntervalMs_;
    std::vector<MemorySource> memorySources_;
    std::vector<std::function<void()>> criticalCallbacks_;
    std::thread monitoringThread_;
    std::mutex mutex_;
    std::atomic<bool> shouldStop_{false};
    int nextSourceId_{0};
    
    // Singleton instance
    static std::unique_ptr<ServiceMemoryMonitor> instance_;
    static std::once_flag initFlag_;
}; 