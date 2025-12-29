/* Copyright 2025 Logzilla Corp. */

#pragma once
#include "BitmappedObjectPool.h"
#include "BitmappedObjectPoolImpl.h"
#include "ResourceMonitor.h"
#include <memory>
#include <string>

/**
 * Helper functions to create ResourceMonitor instances for different types of pools
 */
namespace MemoryMonitoring {

    /**
     * Create a ResourceMonitor for memory buffers with the recommended thresholds
     * 
     * @param poolName Name to identify this pool in logs
     * @param logFormat Format string for log messages (should contain %d for the count value)
     * @param hysteresisPercent Percentage below threshold to consider it "uncrossed"
     * @return Unique_ptr to a configured ResourceMonitor
     */
    inline std::unique_ptr<ResourceMonitor> createMessageBufferMonitor(
        const std::string& poolName,
        const std::string& logFormat = "%s buffer count: %d",
        int hysteresisPercent = 20
    ) {
        return std::make_unique<ResourceMonitor>(
            poolName,
            logFormat,
            std::make_tuple(0, 0),        // Debug: disabled
            std::make_tuple(0, 0),        // Verbose: disabled
            std::make_tuple(10, 100),     // Info: log every 10 allocations up to 100
            std::make_tuple(100, 1000),   // Warning: log every 100 allocations up to 1000
            std::make_tuple(1000, 10000), // RecoverableError: log every 1000 allocations up to 10000
            std::make_tuple(10000, 100000), // CriticalError: log every 10000 allocations up to 100000
            std::make_tuple(0, 0),        // FatalError: disabled, let the program crash naturally
            hysteresisPercent              // Only consider a threshold "uncrossed" after dropping 20% below it
        );
    }

    /**
     * Create a ResourceMonitor for event objects with custom thresholds
     * 
     * @param poolName Name to identify this pool in logs
     * @param logFormat Format string for log messages (should contain %d for the count value)
     * @param maxWarningLevel Maximum count before warning level is triggered
     * @param hysteresisPercent Percentage below threshold to consider it "uncrossed"
     * @return Unique_ptr to a configured ResourceMonitor
     */
    inline std::unique_ptr<ResourceMonitor> createEventObjectMonitor(
        const std::string& poolName, 
        const std::string& logFormat = "%s event count: %d",
        int maxWarningLevel = 5000,
        int hysteresisPercent = 20
    ) {
        return std::make_unique<ResourceMonitor>(
            poolName,
            logFormat,
            std::make_tuple(0, 0),        // Debug: disabled
            std::make_tuple(0, 0),        // Verbose: disabled
            std::make_tuple(10, 100),     // Info: log every 10 allocations up to 100
            std::make_tuple(100, maxWarningLevel),  // Warning: log every 100 allocations up to maxWarningLevel
            std::make_tuple(maxWarningLevel, maxWarningLevel * 2), // RecoverableError: double the warning level
            std::make_tuple(maxWarningLevel * 2, maxWarningLevel * 5), // CriticalError: 5x the warning level
            std::make_tuple(0, 0),        // FatalError: disabled, let the program crash naturally
            hysteresisPercent              // Only consider a threshold "uncrossed" after dropping 20% below it
        );
    }

    /**
     * Attach a memory buffer monitor to an existing pool
     * 
     * @param pool Reference to the pool to monitor
     * @param poolName Name to identify this pool in logs
     * @param logFormat Format string for log messages (should contain %d for the count value)
     * @param hysteresisPercent Percentage below threshold to consider it "uncrossed"
     * @return Unique_ptr to the created ResourceMonitor (caller must keep alive)
     */
    template <typename T>
    inline std::unique_ptr<ResourceMonitor> attachMessageBufferMonitor(
        BitmappedObjectPool<T>& pool,
        const std::string& poolName,
        const std::string& logFormat = "%s buffer count: %d",
        int hysteresisPercent = 20
    ) {
        auto monitor = createMessageBufferMonitor(poolName, logFormat, hysteresisPercent);
        pool.setResourceMonitor(monitor.get());
        return monitor;
    }

    /**
     * Attach an event object monitor to an existing pool
     * 
     * @param pool Reference to the pool to monitor
     * @param poolName Name to identify this pool in logs
     * @param logFormat Format string for log messages (should contain %d for the count value)
     * @param maxWarningLevel Maximum count before warning level is triggered
     * @param hysteresisPercent Percentage below threshold to consider it "uncrossed"
     * @return Unique_ptr to the created ResourceMonitor (caller must keep alive)
     */
    template <typename T>
    inline std::unique_ptr<ResourceMonitor> attachEventObjectMonitor(
        BitmappedObjectPool<T>& pool,
        const std::string& poolName,
        const std::string& logFormat = "%s event count: %d",
        int maxWarningLevel = 5000,
        int hysteresisPercent = 20
    ) {
        auto monitor = createEventObjectMonitor(poolName, logFormat, maxWarningLevel, hysteresisPercent);
        pool.setResourceMonitor(monitor.get());
        return monitor;
    }

} // namespace MemoryMonitoring

/**
 * Usage example:
 *
 * // In your class header:
 * class MyClass {
 * private:
 *     BitmappedObjectPool<char> buffer_pool_;
 *     std::unique_ptr<ResourceMonitor> buffer_monitor_;
 * };
 *
 * // In your class implementation:
 * MyClass::MyClass() : buffer_pool_(1024, 50) {
 *     // Create and attach a monitor to the pool
 *     buffer_monitor_ = MemoryMonitoring::attachMessageBufferMonitor(
 *         buffer_pool_, 
 *         "MyClass",
 *         "MyClass buffer count: %d");
 * }
 */ 