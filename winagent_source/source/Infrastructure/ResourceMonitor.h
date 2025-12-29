/* Copyright 2025 Logzilla Corp. */

#pragma once
#ifdef INFRASTRUCTURE_STATIC
#define INFRA_API
#else
#ifdef INFRASTRUCTURE_EXPORTS
#define INFRA_API __declspec(dllexport)
#else
#define INFRA_API __declspec(dllimport)
#endif
#endif

#include <vector>
#include <tuple>
#include <functional>
#include "Logger.h"
#include <mutex>
#include <string>
#include <unordered_map>
#include <algorithm>

/**
 * ResourceMonitor - A class for monitoring resource consumption with tiered logging thresholds.
 *
 * Provides a way to monitor resource consumption with different logging levels triggered
 * at configurable thresholds. This is primarily used with BitmappedObjectPool to track
 * memory consumption and alert when it exceeds expected thresholds.
 *
 * USAGE GUIDELINES:
 * 1. Every monitor should have a UNIQUE DESCRIPTIVE NAME that clearly identifies:
 *    - Which component owns it (e.g., "PrimaryMessageQueue", "EventHandler")
 *    - What type of objects it contains
 *    - Any instance identifier if multiple instances exist
 * 
 * 2. Log formats should follow this pattern: "Component[Instance]-ObjectType count: %d"
 *    - Example: "MessageQueue[Primary]-Buffers count: %d"
 *
 * 3. Monitor instances should be kept alive for the lifetime of the pool they monitor
 */
class INFRA_API ResourceMonitor {
public:
    enum LogLevel {
        Debug,
        Verbose,
        Info,
        Warning,
        RecoverableError,
        CriticalError,
        FatalError
    };

    /**
     * Constructor with logging thresholds
     * 
     * @param name Name of the resource being monitored
     * @param logFormat Format string for log messages (should contain %d for the count value)
     * @param debugThresholds Increments and max for debug level logs
     * @param verboseThresholds Increments and max for verbose level logs
     * @param infoThresholds Increments and max for info level logs
     * @param warningThresholds Increments and max for warning level logs
     * @param errorThresholds Increments and max for recoverable error level logs
     * @param criticalThresholds Increments and max for critical error level logs
     * @param fatalThresholds Increments and max for fatal error level logs
     * @param hysteresisPercent Percentage below threshold to consider it "uncrossed" (prevents oscillating logs)
     */
    ResourceMonitor(
        const std::string& name,
        const std::string& logFormat,
        std::tuple<int, int> debugThresholds,
        std::tuple<int, int> verboseThresholds,
        std::tuple<int, int> infoThresholds,
        std::tuple<int, int> warningThresholds,
        std::tuple<int, int> errorThresholds,
        std::tuple<int, int> criticalThresholds,
        std::tuple<int, int> fatalThresholds,
        int hysteresisPercent = 20
    );

    /**
     * Called when a resource is consumed/allocated
     * 
     * @param currentCount Current count of resources in use
     * @param totalCount Total capacity of resources
     * @return The monitoring token for future reference
     */
    void* resourceConsumed(int currentCount, int totalCount);

    /**
     * Called when a resource is returned/deallocated
     * 
     * @param currentCount Current count of resources in use after return
     * @param totalCount Total capacity of resources
     * @return The monitoring token for future reference
     */
    void* resourceReturned(int currentCount, int totalCount);

    /**
     * Get the last reported count
     */
    int getLastCount() const { return lastCount_; }
    
    /**
     * Get the monitor name
     */
    std::string getName() const { return name_; }
    
    /**
     * Check if usage has been continuously increasing
     * @param checkThreshold Number of consecutive increases to check for
     * @return true if usage has been continuously increasing
     */
    bool isUsageContinuouslyIncreasing(int checkThreshold = 5) const;
    
    /**
     * Dump the current state to the log
     * @param logLevel Level to log at
     */
    void dumpState(LogLevel logLevel = Info) const;

private:
    void logIfNeeded(int currentCount, int totalCount, bool isConsumption);
    void checkThresholds(int currentCount, LogLevel level, int increment, int max, bool isConsumption);
    bool hasThresholdReallyBeenCrossed(int currentCount, int threshold, int& lastNotification, bool crossingUp);

    std::string name_;
    std::string logFormat_;
    int lastCount_;
    int hysteresisPercent_;
    
    // Thresholds for each log level: (increment, max)
    std::tuple<int, int> debugThresholds_;
    std::tuple<int, int> verboseThresholds_;
    std::tuple<int, int> infoThresholds_;
    std::tuple<int, int> warningThresholds_;
    std::tuple<int, int> errorThresholds_;
    std::tuple<int, int> criticalThresholds_;
    std::tuple<int, int> fatalThresholds_;
    
    // Tracking for the last notification we sent at each level
    // Used for hysteresis calculations
    int lastDebugNotification_;
    int lastVerboseNotification_;
    int lastInfoNotification_;
    int lastWarningNotification_;
    int lastErrorNotification_;
    int lastCriticalNotification_;
    int lastFatalNotification_;
    
    // Track if we've already logged crossing specific thresholds (to add hysteresis)
    bool debugThresholdCrossed_ = false;
    bool verboseThresholdCrossed_ = false;
    bool infoThresholdCrossed_ = false;
    bool warningThresholdCrossed_ = false;
    bool errorThresholdCrossed_ = false;
    bool criticalThresholdCrossed_ = false;
    bool fatalThresholdCrossed_ = false;
    
    // History for continuous increase detection
    std::vector<int> countHistory_;
    mutable std::mutex historyMutex_;
}; 