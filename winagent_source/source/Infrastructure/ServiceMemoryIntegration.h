/* Copyright 2025 Logzilla Corp. */

#pragma once
#include "ServiceMemoryMonitor.h"
#include "BitmappedObjectPool.h"
#include "Logger.h"
#include <functional>

/**
 * Integration Helpers for ServiceMemoryMonitor 
 * 
 * This file contains utility functions to integrate the ServiceMemoryMonitor
 * with various parts of the SyslogAgent service.
 */
namespace MemoryMonitoring {

/**
 * Register a BitmappedObjectPool with the ServiceMemoryMonitor
 * 
 * @param name Display name for this pool
 * @param pool Reference to the pool to monitor
 * @param warningThreshold Number of allocated objects that triggers a warning
 * @param criticalThreshold Number of allocated objects that triggers a critical alert
 * @return The source ID for the registered monitor
 */
template<typename T>
int registerPoolMonitor(
    const std::string& name,
    BitmappedObjectPool<T>& pool,
    size_t warningThreshold,
    size_t criticalThreshold
) {
    auto countFunc = [&pool]() -> size_t {
        return pool.countBuffers();
    };
    
    return ServiceMemoryMonitor::instance().registerMemorySource(
        name, countFunc, warningThreshold, criticalThreshold);
}

/**
 * Register a memory cleanup action when memory is critical
 * 
 * @param cleanupFunc Function to call when memory usage is critical
 */
inline void registerCriticalMemoryAction(std::function<void()> cleanupFunc) {
    ServiceMemoryMonitor::instance().registerCriticalMemoryCallback(cleanupFunc);
}

/**
 * Initialize memory monitoring for the service
 * 
 * Call this during service startup to set up memory monitoring.
 * 
 * @param pollIntervalMs How often to check memory usage (in milliseconds)
 */
inline void initializeMemoryMonitoring(int pollIntervalMs = 30000) {
    auto& monitor = ServiceMemoryMonitor::instance();
    
    // Register critical memory actions
    registerCriticalMemoryAction([]() {
        auto logger = LOG_THIS;
        logger->info("Running garbage collection on all pools due to critical memory usage");
        
        // TODO: Add calls to forceGarbageCollection() on critical pools
        
        // Force a logger flush to ensure messages are written
        logger->forceFlush();
    });
    
    // Start monitoring
    monitor.startMonitoring();
    
    auto logger = LOG_THIS;
    logger->info("Memory monitoring initialized with %d ms poll interval", pollIntervalMs);
}

/**
 * Shutdown memory monitoring
 * 
 * Call this during service shutdown
 */
inline void shutdownMemoryMonitoring() {
    ServiceMemoryMonitor::instance().stopMonitoring();
    
    auto logger = LOG_THIS;
    logger->info("Memory monitoring stopped");
}

} // namespace MemoryMonitoring

/**
 * Usage example:
 * 
 * // In Service.cpp's Initialize method:
 * void Service::Initialize() {
 *     // ... existing initialization code ...
 *     
 *     // Set up memory monitoring
 *     MemoryMonitoring::initializeMemoryMonitoring();
 *     
 *     // Register pools to monitor (typically after they're created)
 *     MemoryMonitoring::registerPoolMonitor(
 *         "Global Message Buffers", 
 *         *Globals::instance()->getMessageBuffersPool(),
 *         100,  // Warning at 100 buffers
 *         500); // Critical at 500 buffers
 *         
 *     // ... continue with initialization ...
 * }
 * 
 * // In Service.cpp's Shutdown method:
 * void Service::Shutdown() {
 *     // ... existing shutdown code ...
 *     
 *     // Stop memory monitoring
 *     MemoryMonitoring::shutdownMemoryMonitoring();
 *     
 *     // ... continue with shutdown ...
 * }
 */ 