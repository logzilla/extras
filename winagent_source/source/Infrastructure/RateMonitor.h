#pragma once

#include <chrono>
#include <mutex>
#include <cstddef>
#include "Logger.h"  // Assumes Logger has methods such as debug(), warning(), etc.

///////////////////////////////////////////////////////////////////////////////
// RateTracker
///////////////////////////////////////////////////////////////////////////////

class RateTracker {
public:
    RateTracker()
        : count_(0), startTime_(std::chrono::steady_clock::now())
    {
    }

    // Record a single event occurrence.
    void recordEvent() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++count_;
    }

    // Record a number of events at once.
    void recordEvents(std::size_t n) {
        std::lock_guard<std::mutex> lock(mutex_);
        count_ += n;
    }

    // Return the current rate in events per second, computed over the elapsed time.
    double getRate() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - startTime_;
        return (elapsed.count() > 0.0) ? static_cast<double>(count_) / elapsed.count() : 0.0;
    }

    // Optionally reset the tracker.
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        count_ = 0;
        startTime_ = std::chrono::steady_clock::now();
    }

private:
    mutable std::mutex mutex_;
    std::size_t count_;
    std::chrono::steady_clock::time_point startTime_;
};

///////////////////////////////////////////////////////////////////////////////
// Metrics (Singleton)
///////////////////////////////////////////////////////////////////////////////

class Metrics {
public:
    // Get the global Metrics instance.
    static Metrics& instance() {
        static Metrics inst;
        return inst;
    }

    // Tracker for incoming events (e.g. each processed Windows event).
    RateTracker incoming;

    // Tracker for outgoing messages (e.g. each batch or each individual message sent).
    RateTracker outgoing;

    // Check the two rates. If the incoming rate exceeds the outgoing rate by more
    // than 'threshold_ratio', log a warning.
    // For example, threshold_ratio = 1.2 means if incoming > 1.2 * outgoing, warn.
    void checkRates(double threshold_ratio = 1.2) {
        double inRate = incoming.getRate();
        double outRate = outgoing.getRate();
        if (inRate > outRate * threshold_ratio) {
            Logger::debug("RateMonitor: Incoming rate %.2f events/s exceeds outgoing rate %.2f events/s (threshold ratio %.2f)",
                inRate, outRate, threshold_ratio);
        }
    }

private:
    // Private constructor to enforce singleton semantics.
    Metrics() {}
    Metrics(const Metrics&) = delete;
    Metrics& operator=(const Metrics&) = delete;
};
