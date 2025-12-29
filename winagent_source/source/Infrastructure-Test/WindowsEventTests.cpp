#include "pch.h"
#include "../Infrastructure/WindowsEvent.h"
#include <thread>
#include <chrono>
#include <mutex>

class WindowsEventTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a unique event name for each test to avoid conflicts
        static int counter = 0;
        std::wstring uniqueName = L"WindowsEventTest_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(counter++);
        testEvent = new WindowsEvent(uniqueName);
    }

    void TearDown() override {
        delete testEvent;
    }

    WindowsEvent* testEvent;
    std::mutex testMutex;
};

// Test basic signal and wait functionality
TEST_F(WindowsEventTest, BasicSignalAndWait) {
    // Create a thread that waits for the event
    bool eventSignaled = false;
    std::thread waitThread([&]() {
        // Wait with a timeout of 1 second
        eventSignaled = testEvent->wait(1000);
    });

    // Signal the event from the main thread
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    testEvent->signal();
    
    // Wait for the thread to finish
    waitThread.join();
    
    // Check that the event was successfully signaled and detected
    EXPECT_TRUE(eventSignaled);
}

// Test wait timeout functionality
TEST_F(WindowsEventTest, WaitTimeout) {
    // Wait with a very short timeout and verify it times out
    bool result = testEvent->wait(50);
    EXPECT_FALSE(result);
}

// Test reset functionality
TEST_F(WindowsEventTest, ResetAfterSignal) {
    // Signal the event
    testEvent->signal();
    
    // Verify it was signaled
    bool firstWait = testEvent->wait(0);
    EXPECT_TRUE(firstWait);
    
    // Reset the event
    testEvent->reset();
    
    // Verify it's no longer signaled
    bool secondWait = testEvent->wait(0);
    EXPECT_FALSE(secondWait);
}

// Test multiple signals without reset
TEST_F(WindowsEventTest, MultipleSignalsWithoutReset) {
    // Signal the event multiple times
    testEvent->signal();
    testEvent->signal();
    testEvent->signal();
    
    // Verify it remains signaled after multiple wait calls
    EXPECT_TRUE(testEvent->wait(0));
    EXPECT_TRUE(testEvent->wait(0));
    EXPECT_TRUE(testEvent->wait(0));
}

// Test closing the event handle
TEST_F(WindowsEventTest, CloseHandle) {
    testEvent->close();
    
    // Wait should fail after close
    bool result = testEvent->wait(0);
    EXPECT_FALSE(result);
}

// Test multiple threads waiting on the same event
TEST_F(WindowsEventTest, MultipleWaiters) {
    constexpr int THREAD_COUNT = 5;
    int signaled_count = 0;
    std::thread threads[THREAD_COUNT];
    
    // Create multiple threads waiting on the same event
    for (int i = 0; i < THREAD_COUNT; i++) {
        threads[i] = std::thread([&, i]() {
            if (testEvent->wait(1000)) {
                std::lock_guard<std::mutex> lock(testMutex);
                signaled_count++;
            }
        });
    }
    
    // Signal the event
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    testEvent->signal();
    
    // Wait for all threads to complete
    for (int i = 0; i < THREAD_COUNT; i++) {
        threads[i].join();
    }
    
    // All threads should have been signaled
    EXPECT_EQ(signaled_count, THREAD_COUNT);
}
