#if ONLY_FOR_DEBUGGING_CURRENTLY_DISABLED

#pragma once

#include <chrono>
#include <mutex>
#include <deque>
#include <cstddef>
#include "Logger.h"  // Assumes Logger::debug(), Logger::warning(), etc.

///////////////////////////////////////////////////////////////////////////////
// SlidingWindowRateTracker
///////////////////////////////////////////////////////////////////////////////

class SlidingWindowRateTracker {
public:
    // Constructor with a configurable window duration.
    // Default is 60 seconds.
    explicit SlidingWindowRateTracker(std::chrono::seconds window_duration = std::chrono::seconds(60))
        : window_duration_(window_duration)
    {
    }

    // Record an event by pushing the current timestamp.
    void recordEvent() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        events_.push_back(now);
        purgeOldEvents(now);
    }

    // Return the current rate (events per second) computed over the sliding window.
    double getRate() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        purgeOldEvents(now);
        const size_t count = events_.size();
        // Convert window duration to seconds (as a double)
        double windowSecs = std::chrono::duration_cast<std::chrono::duration<double>>(window_duration_).count();
        return windowSecs > 0.0 ? static_cast<double>(count) / windowSecs : 0.0;
    }

    // Return the current count of events in the window.
    size_t getCount() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        purgeOldEvents(now);
        return events_.size();
    }

    // Set a new window duration. (This affects both future purges and subsequent rate calculations.)
    void setWindowDuration(std::chrono::seconds duration) {
        std::lock_guard<std::mutex> lock(mutex_);
        window_duration_ = duration;
        purgeOldEvents(std::chrono::steady_clock::now());
    }

    // Reset the tracker by clearing all stored events.
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.clear();
    }

private:
    // Remove any events older than (now - window_duration_).
    void purgeOldEvents(const std::chrono::steady_clock::time_point& now) {
        while (!events_.empty() && (now - events_.front() > window_duration_)) {
            events_.pop_front();
        }
    }

    std::chrono::seconds window_duration_;
    std::deque<std::chrono::steady_clock::time_point> events_;
    std::mutex mutex_;
};

///////////////////////////////////////////////////////////////////////////////
// SlidingWindowMetrics (Singleton)
///////////////////////////////////////////////////////////////////////////////

class SlidingWindowMetrics {
public:
    // Return the singleton instance.
    static SlidingWindowMetrics& instance();

    // Record an incoming event (for example, a processed Windows event).
    void recordIncoming() {
        incoming_.recordEvent();
    }

    // Record an outgoing event (for example, each message or batch sent).
    void recordOutgoing() {
        outgoing_.recordEvent();
    }

    // Get the current incoming rate (events per second).
    double incomingRate() {
        return incoming_.getRate();
    }

    // Get the current outgoing rate (events per second).
    double outgoingRate() {
        return outgoing_.getRate();
    }

    // Compare rates. If the incoming rate exceeds the outgoing rate by more than
    // the specified threshold_ratio, log a warning.
    bool checkRates(double threshold_ratio = 1.2) {
        double inRate = incomingRate();
        double outRate = outgoingRate();
        return (inRate > outRate * threshold_ratio);
    }

    // Set the sliding window duration for both trackers.
    void setWindowDuration(int seconds) {
		std::chrono::seconds duration(seconds);
        incoming_.setWindowDuration(duration);
        outgoing_.setWindowDuration(duration);
    }

    // Optionally reset both trackers.
    void reset() {
        incoming_.reset();
        outgoing_.reset();
    }

private:
    // Private constructor for singleton semantics.
    SlidingWindowMetrics()
        : incoming_(std::chrono::seconds(60)), outgoing_(std::chrono::seconds(60))
    {
    }
    SlidingWindowMetrics(const SlidingWindowMetrics&) = delete;
    SlidingWindowMetrics& operator=(const SlidingWindowMetrics&) = delete;

    SlidingWindowRateTracker incoming_;
    SlidingWindowRateTracker outgoing_;
};

#endif
