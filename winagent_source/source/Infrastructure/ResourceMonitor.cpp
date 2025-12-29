/* Copyright 2025 Logzilla Corp. */

#include "pch.h"
#include "ResourceMonitor.h"
#include "Logger.h"
#include <algorithm>
#include <sstream>

ResourceMonitor::ResourceMonitor(
    const std::string& name,
    const std::string& logFormat,
    std::tuple<int, int> debugThresholds,
    std::tuple<int, int> verboseThresholds,
    std::tuple<int, int> infoThresholds,
    std::tuple<int, int> warningThresholds,
    std::tuple<int, int> errorThresholds,
    std::tuple<int, int> criticalThresholds,
    std::tuple<int, int> fatalThresholds,
    int hysteresisPercent
) : 
    name_(name),
    logFormat_(logFormat.empty() ? "%s resource count: %d" : logFormat),
    lastCount_(0),
    hysteresisPercent_(hysteresisPercent),
    debugThresholds_(debugThresholds),
    verboseThresholds_(verboseThresholds),
    infoThresholds_(infoThresholds),
    warningThresholds_(warningThresholds),
    errorThresholds_(errorThresholds),
    criticalThresholds_(criticalThresholds),
    fatalThresholds_(fatalThresholds),
    lastDebugNotification_(0),
    lastVerboseNotification_(0),
    lastInfoNotification_(0),
    lastWarningNotification_(0),
    lastErrorNotification_(0),
    lastCriticalNotification_(0),
    lastFatalNotification_(0)
{
    auto logger = LOG_THIS;
    logger->debug("ResourceMonitor initialized for '%s'", name_.c_str());
}

void* ResourceMonitor::resourceConsumed(int currentCount, int totalCount) {
    logIfNeeded(currentCount, totalCount, true);
    lastCount_ = currentCount;
    return this;
}

void* ResourceMonitor::resourceReturned(int currentCount, int totalCount) {
    logIfNeeded(currentCount, totalCount, false);
    lastCount_ = currentCount;
    return this;
}

bool ResourceMonitor::hasThresholdReallyBeenCrossed(int currentCount, int threshold, int& lastNotification, bool crossingUp) {
    // Get the relevant threshold crossed flag based on the log level
    bool& thresholdCrossed = [&]() -> bool& {
        if (&lastNotification == &lastDebugNotification_) return debugThresholdCrossed_;
        if (&lastNotification == &lastVerboseNotification_) return verboseThresholdCrossed_;
        if (&lastNotification == &lastInfoNotification_) return infoThresholdCrossed_;
        if (&lastNotification == &lastWarningNotification_) return warningThresholdCrossed_;
        if (&lastNotification == &lastErrorNotification_) return errorThresholdCrossed_;
        if (&lastNotification == &lastCriticalNotification_) return criticalThresholdCrossed_;
        if (&lastNotification == &lastFatalNotification_) return fatalThresholdCrossed_;
        static bool dummy = false;
        return dummy;
    }();
    
    if (crossingUp) {
        // We're crossing the threshold going up
        if (currentCount >= threshold && !thresholdCrossed) {
            thresholdCrossed = true;
            return true;
        }
    } else {
        // We're crossing the threshold going down (need hysteresis)
        if (thresholdCrossed) {
            // Calculate the hysteresis threshold (percentage below the max)
            int hysteresisThreshold = threshold - (threshold * hysteresisPercent_ / 100);
            if (currentCount <= hysteresisThreshold) {
                // Count has dropped enough below the threshold to consider it uncrossed
                thresholdCrossed = false;
            }
        }
    }
    
    return false;
}

void ResourceMonitor::logIfNeeded(int currentCount, int totalCount, bool isConsumption) {
    // Check each threshold from lowest to highest priority
    checkThresholds(currentCount, LogLevel::Debug, 
                   std::get<0>(debugThresholds_), 
                   std::get<1>(debugThresholds_), 
                   isConsumption);
    
    checkThresholds(currentCount, LogLevel::Verbose, 
                   std::get<0>(verboseThresholds_), 
                   std::get<1>(verboseThresholds_), 
                   isConsumption);
    
    checkThresholds(currentCount, LogLevel::Info, 
                   std::get<0>(infoThresholds_), 
                   std::get<1>(infoThresholds_), 
                   isConsumption);
    
    checkThresholds(currentCount, LogLevel::Warning, 
                   std::get<0>(warningThresholds_), 
                   std::get<1>(warningThresholds_), 
                   isConsumption);
    
    checkThresholds(currentCount, LogLevel::RecoverableError, 
                   std::get<0>(errorThresholds_), 
                   std::get<1>(errorThresholds_), 
                   isConsumption);
    
    checkThresholds(currentCount, LogLevel::CriticalError, 
                   std::get<0>(criticalThresholds_), 
                   std::get<1>(criticalThresholds_), 
                   isConsumption);
    
    checkThresholds(currentCount, LogLevel::FatalError, 
                   std::get<0>(fatalThresholds_), 
                   std::get<1>(fatalThresholds_), 
                   isConsumption);
}

void ResourceMonitor::checkThresholds(int currentCount, LogLevel level, int increment, int max, bool isConsumption) {
    auto logger = LOG_THIS;
    
    // Skip if max threshold is 0 (disabled for this level)
    if (max <= 0) return;
    
    int& lastNotification = [&]() -> int& {
        switch (level) {
            case LogLevel::Debug: return lastDebugNotification_;
            case LogLevel::Verbose: return lastVerboseNotification_;
            case LogLevel::Info: return lastInfoNotification_;
            case LogLevel::Warning: return lastWarningNotification_;
            case LogLevel::RecoverableError: return lastErrorNotification_;
            case LogLevel::CriticalError: return lastCriticalNotification_;
            case LogLevel::FatalError: return lastFatalNotification_;
            default: return lastDebugNotification_; // Fallback
        }
    }();
    
    // Only log if we've crossed an increment threshold or max
    bool shouldLog = false;
    
    // Check if we just crossed an increment boundary
    if (increment > 0) {
        int lastIncrement = (lastNotification / increment) * increment;
        int currentIncrement = (currentCount / increment) * increment;
        
        // Check increasing increments
        if (currentIncrement > lastIncrement && currentCount <= max) {
            shouldLog = true;
        }
    }
    
    // Always log if we just crossed the max threshold, but apply hysteresis
    // to prevent oscillating messages near the threshold
    if (currentCount >= max) {
        shouldLog = shouldLog || hasThresholdReallyBeenCrossed(currentCount, max, lastNotification, true);
    } else {
        // We're below the max, check if we need to reset the crossed flag with hysteresis
        hasThresholdReallyBeenCrossed(currentCount, max, lastNotification, false);
    }
    
    if (shouldLog) {
        // Create the log message using the provided format
        std::string message;
        
        if (currentCount >= max) {
            message = logFormat_ + " (MAX THRESHOLD: " + std::to_string(max) + ")";
        } else if (increment > 0) {
            int nextIncrement = ((currentCount / increment) + 1) * increment;
            message = logFormat_ + " (next at " + std::to_string(nextIncrement) + ")";
        } else {
            message = logFormat_;
        }
        
        switch (level) {
            case LogLevel::Debug:
                logger->debug(message.c_str(), name_.c_str(), currentCount);
                break;
            case LogLevel::Verbose:
                logger->verbose(message.c_str(), name_.c_str(), currentCount);
                break;
            case LogLevel::Info:
                logger->info(message.c_str(), name_.c_str(), currentCount);
                break;
            case LogLevel::Warning:
                logger->warning(message.c_str(), name_.c_str(), currentCount);
                break;
            case LogLevel::RecoverableError:
                logger->recoverable_error(message.c_str(), name_.c_str(), currentCount);
                break;
            case LogLevel::CriticalError:
                logger->critical(message.c_str(), name_.c_str(), currentCount);
                break;
            case LogLevel::FatalError:
                logger->fatal(message.c_str(), name_.c_str(), currentCount);
                break;
        }
        
        lastNotification = currentCount;
    }
}

bool ResourceMonitor::isUsageContinuouslyIncreasing(int checkThreshold) const {
    auto logger = LOG_THIS;
    
    // We don't have historical data in this implementation yet
    // This would require storing a history of counts
    // For now, we just log a warning if we're at a high percentage of any threshold
    
    // Check if we're close to any max threshold
    int warningMax = std::get<1>(warningThresholds_);
    int errorMax = std::get<1>(errorThresholds_);
    int criticalMax = std::get<1>(criticalThresholds_);
    
    // Skip if thresholds are disabled (0)
    if (warningMax <= 0 && errorMax <= 0 && criticalMax <= 0) {
        return false;
    }
    
    // Check highest percentage of threshold
    float percentOfMax = 0.0f;
    
    if (warningMax > 0) {
        percentOfMax = (std::max)(percentOfMax, (float)lastCount_ / warningMax);
    }
    
    if (errorMax > 0) {
        percentOfMax = (std::max)(percentOfMax, (float)lastCount_ / errorMax);
    }
    
    if (criticalMax > 0) {
        percentOfMax = (std::max)(percentOfMax, (float)lastCount_ / criticalMax);
    }
    
    // If we're over 80% of any threshold, consider this suspicious
    if (percentOfMax > 0.8f) {
        logger->warning("%s usage is at %.1f%% of its threshold - possible continuous increase",
                    name_.c_str(), percentOfMax * 100.0f);
        return true;
    }
    
    return false;
}

void ResourceMonitor::dumpState(LogLevel logLevel) const {
    auto logger = LOG_THIS;
    
    // Format a detailed dump based on the log level
    std::stringstream ss;
    ss << "ResourceMonitor[" << name_ << "] state dump:";
    ss << "\n  Current count: " << lastCount_;
    ss << "\n  Monitoring thresholds:";
    
    // Only show enabled thresholds
    if (std::get<1>(debugThresholds_) > 0) {
        ss << "\n    Debug: increment=" << std::get<0>(debugThresholds_) 
           << ", max=" << std::get<1>(debugThresholds_) 
           << ", last notification=" << lastDebugNotification_
           << ", crossed=" << (debugThresholdCrossed_ ? "yes" : "no");
    }
    
    if (std::get<1>(verboseThresholds_) > 0) {
        ss << "\n    Verbose: increment=" << std::get<0>(verboseThresholds_) 
           << ", max=" << std::get<1>(verboseThresholds_) 
           << ", last notification=" << lastVerboseNotification_
           << ", crossed=" << (verboseThresholdCrossed_ ? "yes" : "no");
    }
    
    if (std::get<1>(infoThresholds_) > 0) {
        ss << "\n    Info: increment=" << std::get<0>(infoThresholds_) 
           << ", max=" << std::get<1>(infoThresholds_) 
           << ", last notification=" << lastInfoNotification_
           << ", crossed=" << (infoThresholdCrossed_ ? "yes" : "no");
    }
    
    if (std::get<1>(warningThresholds_) > 0) {
        ss << "\n    Warning: increment=" << std::get<0>(warningThresholds_) 
           << ", max=" << std::get<1>(warningThresholds_) 
           << ", last notification=" << lastWarningNotification_
           << ", crossed=" << (warningThresholdCrossed_ ? "yes" : "no");
    }
    
    if (std::get<1>(errorThresholds_) > 0) {
        ss << "\n    RecoverableError: increment=" << std::get<0>(errorThresholds_) 
           << ", max=" << std::get<1>(errorThresholds_) 
           << ", last notification=" << lastErrorNotification_
           << ", crossed=" << (errorThresholdCrossed_ ? "yes" : "no");
    }
    
    if (std::get<1>(criticalThresholds_) > 0) {
        ss << "\n    CriticalError: increment=" << std::get<0>(criticalThresholds_) 
           << ", max=" << std::get<1>(criticalThresholds_) 
           << ", last notification=" << lastCriticalNotification_
           << ", crossed=" << (criticalThresholdCrossed_ ? "yes" : "no");
    }
    
    if (std::get<1>(fatalThresholds_) > 0) {
        ss << "\n    FatalError: increment=" << std::get<0>(fatalThresholds_) 
           << ", max=" << std::get<1>(fatalThresholds_) 
           << ", last notification=" << lastFatalNotification_
           << ", crossed=" << (fatalThresholdCrossed_ ? "yes" : "no");
    }
    
    ss << "\n  Hysteresis: " << hysteresisPercent_ << "%";
    
    // Log at the specified level
    switch (logLevel) {
        case Debug:
            logger->debug("%s", ss.str().c_str());
            break;
        case Verbose:
            logger->verbose("%s", ss.str().c_str());
            break;
        case Info:
            logger->info("%s", ss.str().c_str());
            break;
        case Warning:
            logger->warning("%s", ss.str().c_str());
            break;
        case RecoverableError:
            logger->recoverable_error("%s", ss.str().c_str());
            break;
        case CriticalError:
            logger->critical("%s", ss.str().c_str());
            break;
        case FatalError:
            logger->fatal("%s", ss.str().c_str());
            break;
    }
}