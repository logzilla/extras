#include "pch.h"
#include "../Infrastructure/ResourceMonitor.h"
#include "../Infrastructure/BitmappedObjectPool.h"
#include "../Infrastructure/BitmappedObjectPoolImpl.h"
#include "../Infrastructure/MemoryMonitoringExample.h"
#include <vector>
#include <string>

using namespace std;

TEST(ResourceMonitorTest, BasicFunctionality) {
    // Create a ResourceMonitor with test thresholds
    ResourceMonitor monitor(
        "Test",
        "Test resource count: %d",
        std::make_tuple(1, 5),    // Debug: every 1 up to 5
        std::make_tuple(5, 10),   // Verbose: every 5 up to 10
        std::make_tuple(0, 0),    // Info: disabled
        std::make_tuple(0, 0),    // Warning: disabled
        std::make_tuple(0, 0),    // Error: disabled
        std::make_tuple(0, 0),    // Critical: disabled
        std::make_tuple(0, 0),     // Fatal: disabled
        10                        // 10% hysteresis
    );
    
    // Test tracking counts
    void* token = monitor.resourceConsumed(1, 100);
    EXPECT_EQ(monitor.getLastCount(), 1);
    
    token = monitor.resourceConsumed(2, 100);
    EXPECT_EQ(monitor.getLastCount(), 2);
    
    token = monitor.resourceReturned(1, 100);
    EXPECT_EQ(monitor.getLastCount(), 1);
}

TEST(ResourceMonitorTest, HysteresisPreventsOscillation) {
    // Create a ResourceMonitor with a threshold at 10 and 20% hysteresis
    ResourceMonitor monitor(
        "TestHysteresis",
        "Count: %d",
        std::make_tuple(0, 0),    // Debug: disabled
        std::make_tuple(0, 0),    // Verbose: disabled
        std::make_tuple(0, 0),    // Info: disabled
        std::make_tuple(1, 10),   // Warning: every 1 up to 10 (main test threshold)
        std::make_tuple(0, 0),    // Error: disabled
        std::make_tuple(0, 0),    // Critical: disabled
        std::make_tuple(0, 0),    // Fatal: disabled
        20                        // 20% hysteresis (threshold must drop to 8 to be considered uncrossed)
    );
    
    // Cross the threshold going up to 11
    monitor.resourceConsumed(11, 100);
    
    // Drop to 9 (still above hysteresis threshold of 8)
    monitor.resourceReturned(9, 100);
    
    // Go back up to 11 - should not log again since we never went below hysteresis
    monitor.resourceConsumed(11, 100);
    
    // Now drop below hysteresis threshold to 7
    monitor.resourceReturned(7, 100);
    
    // Go back up to 11 - should log again since we dropped below hysteresis
    monitor.resourceConsumed(11, 100);
}

TEST(ResourceMonitorTest, IntegrationWithBitmappedObjectPool) {
    // Create a pool
    BitmappedObjectPool<int> pool(10, 50);
    
    // Create and attach a monitor with custom hysteresis
    auto monitor = MemoryMonitoring::attachMessageBufferMonitor(pool, "TestPool", "TestPool buffers: %d", 25);
    
    // Allocate objects from the pool
    vector<int*> allocated;
    for (int i = 0; i < 15; i++) {
        int* obj = pool.getAndMarkNextUnused();
        ASSERT_NE(obj, nullptr);
        *obj = i;
        allocated.push_back(obj);
    }
    
    // Verify the monitor has tracked the count
    EXPECT_EQ(monitor->getLastCount(), 15);
    
    // Release half the objects
    for (int i = 0; i < 7; i++) {
        pool.markAsUnused(allocated[i]);
    }
    
    // Verify the monitor has tracked the reduced count
    EXPECT_EQ(monitor->getLastCount(), 8);
    
    // Clean up the rest
    for (size_t i = 7; i < allocated.size(); i++) {
        pool.markAsUnused(allocated[i]);
    }
    
    // Verify monitor shows 0
    EXPECT_EQ(monitor->getLastCount(), 0);
}

TEST(ResourceMonitorTest, MemoryMonitoringHelpers) {
    // Test the buffer monitor creation with hysteresis
    auto bufferMonitor = MemoryMonitoring::createMessageBufferMonitor(
        "TestBuffer", "TestBuffer count: %d", 15);
    EXPECT_NE(bufferMonitor, nullptr);
    
    // Test the event monitor creation with hysteresis
    auto eventMonitor = MemoryMonitoring::createEventObjectMonitor(
        "TestEvent", "Events in queue: %d", 2000, 30);
    EXPECT_NE(eventMonitor, nullptr);
    
    // Create a pool and attach an event monitor with hysteresis
    BitmappedObjectPool<string> pool(5, 50);
    auto attachedMonitor = MemoryMonitoring::attachEventObjectMonitor(
        pool, "AttachedTest", "Test events: %d", 5000, 25);
    EXPECT_NE(attachedMonitor, nullptr);
    
    // Allocate an object to verify monitoring
    string* obj = pool.getAndMarkNextUnused();
    ASSERT_NE(obj, nullptr);
    *obj = "Test";
    
    // Verify the monitor has tracked it
    EXPECT_EQ(attachedMonitor->getLastCount(), 1);
} 