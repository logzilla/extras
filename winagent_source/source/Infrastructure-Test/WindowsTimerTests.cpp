#include "pch.h"
#include "../Infrastructure/WindowsTimer.h"
#include <thread>
#include <chrono>

// for some reason this doesn't link, punt for now
#if 0
class WindowsTimerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testTimer = new WindowsTimer();
    }

    void TearDown() override {
        delete testTimer;
    }

    WindowsTimer* testTimer;
};

// Test basic timer functionality
TEST_F(WindowsTimerTest, BasicTimerFunctionality) {
    // Start a timer for 100ms
    testTimer->startTimer(100);
    
    // Timer should not be done immediately
    bool immediateDone = testTimer->waitForTimer(0);
    EXPECT_FALSE(immediateDone);
    
    // Wait for the timer to complete
    bool done = testTimer->waitForTimer(200);
    EXPECT_TRUE(done);
}

// Test stopping a timer
TEST_F(WindowsTimerTest, StopTimer) {
    // Start a timer for 500ms
    testTimer->startTimer(500);
    
    // Timer should not be done immediately
    bool immediateDone = testTimer->waitForTimer(0);
    EXPECT_FALSE(immediateDone);
    
    // Stop the timer before it completes
    testTimer->stopTimer();
    
    // Wait with timeout to see if the timer signals (it shouldn't)
    bool done = testTimer->waitForTimer(100);
    EXPECT_FALSE(done);
}

// Test waiting with timeout
TEST_F(WindowsTimerTest, WaitWithTimeout) {
    // Start a timer for 300ms
    testTimer->startTimer(300);
    
    // Wait with a shorter timeout (100ms)
    auto startTime = std::chrono::steady_clock::now();
    bool done = testTimer->waitForTimer(100);
    auto elapsed = std::chrono::steady_clock::now() - startTime;
    
    // Timer should not be done yet, and we should have waited close to 100ms
    EXPECT_FALSE(done);
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 90);
    EXPECT_LE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 150);
    
    // Now wait until the timer completes
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    done = testTimer->waitForTimer(100);
    EXPECT_TRUE(done);
}

// Test restarting a timer
TEST_F(WindowsTimerTest, RestartTimer) {
    // Start a timer
    testTimer->startTimer(100);
    
    // Start it again before it finishes
    testTimer->startTimer(200);
    
    // Wait a bit (but not long enough for the second timer)
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    bool done = testTimer->waitForTimer(0);
    EXPECT_FALSE(done);
    
    // Now wait for the full second timer
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    done = testTimer->waitForTimer(0);
    EXPECT_TRUE(done);
}

// Test close functionality
TEST_F(WindowsTimerTest, CloseTimer) {
    // Start a timer
    testTimer->startTimer(200);
    
    // Close the timer
    testTimer->close();
    
    // Verify waitForTimer returns false after close
    bool result = testTimer->waitForTimer(0);
    EXPECT_FALSE(result);
    
    // Verify we can start a new timer after closing
    testTimer->startTimer(100);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    result = testTimer->waitForTimer(0);
    EXPECT_TRUE(result);
}

// Test waitForTimer on a timer that hasn't been started
TEST_F(WindowsTimerTest, WaitOnUninitializedTimer) {
    // Don't start the timer
    bool result = testTimer->waitForTimer(100);
    EXPECT_FALSE(result);
}

// Test multiple consecutive timers
TEST_F(WindowsTimerTest, ConsecutiveTimers) {
    // First timer
    testTimer->startTimer(100);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    bool done = testTimer->waitForTimer(0);
    EXPECT_TRUE(done);
    
    // Second timer
    testTimer->startTimer(100);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    done = testTimer->waitForTimer(0);
    EXPECT_TRUE(done);
    
    // Third timer
    testTimer->startTimer(100);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    done = testTimer->waitForTimer(0);
    EXPECT_TRUE(done);
}
#endif