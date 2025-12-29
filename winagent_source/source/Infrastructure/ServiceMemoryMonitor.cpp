/* Copyright 2025 Logzilla Corp. */

#include "pch.h"
#include "ServiceMemoryMonitor.h"
#include <algorithm>

std::unique_ptr<ServiceMemoryMonitor> ServiceMemoryMonitor::instance_ = nullptr;
std::once_flag ServiceMemoryMonitor::initFlag_;

ServiceMemoryMonitor::ServiceMemoryMonitor(int pollIntervalMs)
    : pollIntervalMs_(pollIntervalMs) {
    auto logger = LOG_THIS;
    logger->info("ServiceMemoryMonitor initialized with poll interval: %d ms", pollIntervalMs);
}

ServiceMemoryMonitor::~ServiceMemoryMonitor() {
    stopMonitoring();
}

ServiceMemoryMonitor& ServiceMemoryMonitor::instance() {
    std::call_once(initFlag_, []() {
        instance_ = std::make_unique<ServiceMemoryMonitor>();
    });
    return *instance_;
}

int ServiceMemoryMonitor::registerMemorySource(
    const std::string& name,
    MemoryStatsCallback callback,
    size_t warningThreshold,
    size_t criticalThreshold) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    MemorySource source;
    source.name = name;
    source.callback = callback;
    source.lastValue = 0;
    source.warningThreshold = warningThreshold;
    source.criticalThreshold = criticalThreshold;
    source.enabled = true;
    
    int sourceId = nextSourceId_++;
    memorySources_.push_back(std::move(source));
    
    auto logger = LOG_THIS;
    logger->info("Registered memory source: %s (ID: %d)", name.c_str(), sourceId);
    
    return sourceId;
}

void ServiceMemoryMonitor::unregisterMemorySource(int sourceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (sourceId < 0 || sourceId >= nextSourceId_) {
        return;
    }
    
    auto it = std::find_if(
        memorySources_.begin(),
        memorySources_.end(),
        [sourceId, this](const MemorySource& src) {
            return &src == &memorySources_[sourceId];
        });
    
    if (it != memorySources_.end()) {
        auto logger = LOG_THIS;
        logger->info("Unregistered memory source: %s (ID: %d)", it->name.c_str(), sourceId);
        memorySources_.erase(it);
    }
}

void ServiceMemoryMonitor::startMonitoring() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (monitoringThread_.joinable()) {
        // Already running
        return;
    }
    
    shouldStop_ = false;
    monitoringThread_ = std::thread(&ServiceMemoryMonitor::monitoringThreadFunc, this);
    
    auto logger = LOG_THIS;
    logger->info("Memory monitoring started");
}

void ServiceMemoryMonitor::stopMonitoring() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!monitoringThread_.joinable()) {
            // Not running
            return;
        }
        
        shouldStop_ = true;
    }
    
    monitoringThread_.join();
    
    auto logger = LOG_THIS;
    logger->info("Memory monitoring stopped");
}

void ServiceMemoryMonitor::checkMemoryUsage(bool logAll) {
    auto logger = LOG_THIS;
    std::lock_guard<std::mutex> lock(mutex_);
    
    bool hasCritical = false;
    
    for (auto& source : memorySources_) {
        if (!source.enabled) continue;
        
        try {
            size_t currentValue = source.callback();
            source.lastValue = currentValue;
            
            if (currentValue >= source.criticalThreshold) {
                logger->critical("MEMORY CRITICAL: %s using %zu units (threshold: %zu)",
                              source.name.c_str(), currentValue, source.criticalThreshold);
                hasCritical = true;
            }
            else if (currentValue >= source.warningThreshold) {
                logger->warning("MEMORY WARNING: %s using %zu units (threshold: %zu)",
                             source.name.c_str(), currentValue, source.warningThreshold);
            }
            else if (logAll) {
                logger->info("Memory usage: %s using %zu units",
                          source.name.c_str(), currentValue);
            }
        }
        catch (const std::exception& e) {
            logger->recoverable_error("Error checking memory for %s: %s",
                              source.name.c_str(), e.what());
        }
    }
    
    // If any source is in critical state, execute critical callbacks
    if (hasCritical) {
        for (const auto& callback : criticalCallbacks_) {
            try {
                callback();
            }
            catch (const std::exception& e) {
                logger->recoverable_error("Error in critical memory callback: %s", e.what());
            }
        }
    }
}

void ServiceMemoryMonitor::registerCriticalMemoryCallback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    criticalCallbacks_.push_back(std::move(callback));
}

void ServiceMemoryMonitor::monitoringThreadFunc() {
    auto logger = LOG_THIS;
    logger->debug("Memory monitoring thread started");
    
    while (!shouldStop_) {
        try {
            checkMemoryUsage(false);
        }
        catch (const std::exception& e) {
            logger->recoverable_error("Error in memory monitoring thread: %s", e.what());
        }
        
        // Wait for next monitoring interval or until stopped
        for (int i = 0; i < pollIntervalMs_ / 100 && !shouldStop_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    logger->debug("Memory monitoring thread stopped");
} 