#include "pch.h"
#include "CppUnitTest.h"
#include "../Infrastructure/BitmappedObjectPool.h"
#include "../Infrastructure/PooledObject.h"
#include "../Infrastructure/MessageBufferGuard.h"
#include "../Infrastructure/PoolResourceMonitor.h"
#include "../Infrastructure/IBufferManager.h"
#include <vector>
#include <cstring>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace InfraLib;

// Test data type for BitmappedObjectPool tests
struct TestObject {
    int value;
    char data[100];
};

// Simple buffer manager for testing purposes
struct TestBufferManager : public IBufferManager {
    // Define a fixed buffer size for tests, similar to what Globals might provide
    static const size_t TEST_BUFFER_SIZE = 1024;
    std::vector<char*> allocated_buffers;

    ~TestBufferManager() override {
        // Clean up any buffers that weren't explicitly released (e.g., detached)
        for (char* buf : allocated_buffers) {
            delete[] buf;
        }
    }

    char* allocateBuffer(const char* name) override {
        // In tests, we might not care about the name, just allocate
        char* buffer = new char[TEST_BUFFER_SIZE];
        allocated_buffers.push_back(buffer);
        // Optionally log or track allocations by name if needed for specific tests
        return buffer;
    }

    void releaseBuffer(char* buffer) override {
        // Find the buffer and remove it from tracking, then delete
        for (auto it = allocated_buffers.begin(); it != allocated_buffers.end(); ++it) {
            if (*it == buffer) {
                delete[] buffer;
                allocated_buffers.erase(it);
                return;
            }
        }
        // Optionally Assert or log if trying to release an unknown buffer
    }

    // Helper for tests to check for leaks
    size_t getActiveBufferCount() const {
        return allocated_buffers.size();
    }
};

TEST_CLASS(PooledObjectTests)
{
public:

    TEST_METHOD(TestBasicUsage)
    {
        // Create a pool with chunk size 16 and 0% slack
        BitmappedObjectPool<TestObject> pool(16, 0);

        // Count initial in-use objects
        size_t initial_count = (size_t)pool.countBuffers();

        {
            // Get a pooled object
            PooledObject<TestObject> obj(pool.getAndMarkNextUnused(), pool);

            // Verify we got a valid object
            Assert::IsNotNull(obj.get());

            // Modify the object
            obj->value = 42;
            strcpy_s(obj->data, "Test data");

            // Verify one more buffer is in use
            Assert::AreEqual(initial_count + 1, (size_t)pool.countBuffers());
        }

        // After scope exit, object should be released back to pool
        Assert::AreEqual(initial_count, (size_t)pool.countBuffers());
    }

    TEST_METHOD(TestMoveSemantics)
    {
        BitmappedObjectPool<TestObject> pool(16, 0);
        size_t initial_count = (size_t)pool.countBuffers();

        PooledObject<TestObject> obj1(pool.getAndMarkNextUnused(), pool);
        obj1->value = 42;

        // Move construct
        PooledObject<TestObject> obj2(std::move(obj1));

        // Original should be null after move
        Assert::IsNull(obj1.get());

        // New object should have the value
        Assert::AreEqual(42, obj2->value);

        // Only one buffer should be in use
        Assert::AreEqual(initial_count + 1, (size_t)pool.countBuffers());

        // Move assign
        PooledObject<TestObject> obj3(pool.getAndMarkNextUnused(), pool);
        obj3->value = 99;
        Assert::AreEqual(initial_count + 2, (size_t)pool.countBuffers());

        // Move assign
        obj3 = std::move(obj2);

        // Values should be transferred
        Assert::AreEqual(42, obj3->value);
        Assert::IsNull(obj2.get());

        // One buffer should be freed
        Assert::AreEqual(initial_count + 1, (size_t)pool.countBuffers());
    }

    TEST_METHOD(TestManualRelease)
    {
        BitmappedObjectPool<TestObject> pool(16, 0);
        size_t initial_count = (size_t)pool.countBuffers();

        PooledObject<TestObject> obj(pool.getAndMarkNextUnused(), pool);
        Assert::AreEqual(initial_count + 1, (size_t)pool.countBuffers());

        // Manually release
        obj.release();

        // Should be back to initial count
        Assert::AreEqual(initial_count, (size_t)pool.countBuffers());

        // Object pointer should be null
        Assert::IsNull(obj.get());

        // Calling release again should be safe
        obj.release(); // Should not crash or have any effect
    }

    TEST_METHOD(TestDetach)
    {
        BitmappedObjectPool<TestObject> pool(16, 0);
        size_t initial_count = (size_t)pool.countBuffers();

        TestObject* raw_ptr = nullptr;

        {
            PooledObject<TestObject> obj(pool.getAndMarkNextUnused(), pool);
            Assert::AreEqual(initial_count + 1, (size_t)pool.countBuffers());

            // Setup test data
            obj->value = 42;

            // Detach transfers ownership
            raw_ptr = obj.detach();

            // Original should be null but count unchanged
            Assert::IsNull(obj.get());
            Assert::AreEqual(initial_count + 1, (size_t)pool.countBuffers());
        }

        // After scope exit, count should still be up by one
        // since we detached the pointer
        Assert::AreEqual(initial_count + 1, (size_t)pool.countBuffers());

        // We can still access the detached object
        Assert::AreEqual(42, raw_ptr->value);

        // Clean up manually
        pool.markAsUnused(raw_ptr);
        Assert::AreEqual(initial_count, (size_t)pool.countBuffers());
    }

    TEST_METHOD(TestHelperFunction)
    {
        BitmappedObjectPool<TestObject> pool(16, 0);
        size_t initial_count = (size_t)pool.countBuffers();

        {
            // Get an object directly using the pool method
            TestObject* ptr = pool.getAndMarkNextUnused();
            // Wrap it in PooledObject manually
            PooledObject<TestObject> obj(ptr, pool);

            // Verify we got a valid object
            Assert::IsNotNull(obj.get());

            // Verify one more buffer is in use
            Assert::AreEqual(initial_count + 1, (size_t)pool.countBuffers());
        }

        // After scope exit, object should be released back to pool
        Assert::AreEqual(initial_count, (size_t)pool.countBuffers());
    }
};

TEST_CLASS(MessageBufferGuardTests)
{
    TestBufferManager testManager; // Instance of the test manager for these tests

public:
    TEST_METHOD_INITIALIZE(Initialize)
    {
        // Reset the PoolResourceMonitor stats (still relevant)
        g_PoolResourceMonitor.resetStats();
        // Reset our test manager state if necessary (e.g., clear buffer list)
        // (Handled by destructor/recreation for now, but could add explicit reset)
    }

    TEST_METHOD(TestBasicUsage)
    {
        // Create a MessageBufferGuard using the test manager
        const char* buffer_name = "test_buffer";
        MessageBufferGuard buffer(buffer_name, testManager);

        // Verify we got a valid buffer
        Assert::IsNotNull(buffer.get());

        // We can use it as a char*
        strcpy_s(buffer.get(), TestBufferManager::TEST_BUFFER_SIZE, "Test data");

        // Verify data was written
        Assert::AreEqual("Test data", buffer.get());
    }

    TEST_METHOD(TestAutoRelease)
    {
        // We'll track allocated buffers by name using the ResourceMonitor
        size_t initial_alloc = g_PoolResourceMonitor.getAllocationCount("test_auto");
        size_t initial_release = g_PoolResourceMonitor.getReleaseCount("test_auto");
        size_t initial_test_manager_count = testManager.getActiveBufferCount();

        {
            MessageBufferGuard buffer("test_auto", testManager);

            // Should have one more allocation (tracked by both)
            Assert::AreEqual(initial_alloc + 1, g_PoolResourceMonitor.getAllocationCount("test_auto"));
            Assert::AreEqual(initial_release, g_PoolResourceMonitor.getReleaseCount("test_auto"));
            Assert::AreEqual(initial_test_manager_count + 1, testManager.getActiveBufferCount());
        }

        // After scope exit, buffer should be released
        Assert::AreEqual(initial_alloc + 1, g_PoolResourceMonitor.getAllocationCount("test_auto"));
        Assert::AreEqual(initial_release + 1, g_PoolResourceMonitor.getReleaseCount("test_auto"));
        Assert::AreEqual(initial_test_manager_count, testManager.getActiveBufferCount());
    }

    TEST_METHOD(TestManualRelease)
    {
        size_t initial_alloc = g_PoolResourceMonitor.getAllocationCount("test_manual");
        size_t initial_release = g_PoolResourceMonitor.getReleaseCount("test_manual");
        size_t initial_test_manager_count = testManager.getActiveBufferCount();

        MessageBufferGuard buffer("test_manual", testManager);

        // Should have one more allocation
        Assert::AreEqual(initial_alloc + 1, g_PoolResourceMonitor.getAllocationCount("test_manual"));
        Assert::AreEqual(initial_test_manager_count + 1, testManager.getActiveBufferCount());

        // Manually release
        buffer.release();

        // Should have one more release
        Assert::AreEqual(initial_release + 1, g_PoolResourceMonitor.getReleaseCount("test_manual"));
        Assert::AreEqual(initial_test_manager_count, testManager.getActiveBufferCount());

        // Buffer pointer should be null
        Assert::IsNull(buffer.get());

        // Calling release again should be safe
        buffer.release(); // Should not crash

        // No additional release should be recorded
        Assert::AreEqual(initial_release + 1, g_PoolResourceMonitor.getReleaseCount("test_manual"));
        Assert::AreEqual(initial_test_manager_count, testManager.getActiveBufferCount());
    }

    TEST_METHOD(TestMoveSemantics)
    {
        // Create a second manager to test the cross-manager move prevention
        TestBufferManager testManager2;

        size_t initial_alloc = g_PoolResourceMonitor.getAllocationCount("test_move");
        size_t initial_release = g_PoolResourceMonitor.getReleaseCount("test_move");
        size_t tm1_count = testManager.getActiveBufferCount();

        MessageBufferGuard buffer1("test_move", testManager);
        strcpy_s(buffer1.get(), TestBufferManager::TEST_BUFFER_SIZE, "Test data");
        Assert::AreEqual(tm1_count + 1, testManager.getActiveBufferCount());

        // Move construct
        MessageBufferGuard buffer2(std::move(buffer1));

        // Original should be null after move
        Assert::IsNull(buffer1.get());
        Assert::AreEqual(tm1_count + 1, testManager.getActiveBufferCount()); // Buffer still owned by testManager

        // New object should have the data
        Assert::AreEqual("Test data", buffer2.get());

        // Only one allocation should be recorded by global monitor
        Assert::AreEqual(initial_alloc + 1, g_PoolResourceMonitor.getAllocationCount("test_move"));

        // Create another buffer with the *same* manager
        MessageBufferGuard buffer3("test_move_3", testManager);
        strcpy_s(buffer3.get(), TestBufferManager::TEST_BUFFER_SIZE, "Other data");
        Assert::AreEqual(tm1_count + 2, testManager.getActiveBufferCount());

        // Move assign (same manager - should work)
        buffer3 = std::move(buffer2);

        // buffer2 should be null, buffer3 should have buffer2's data
        Assert::IsNull(buffer2.get());
        Assert::AreEqual("Test data", buffer3.get());

        // One buffer should be released (buffer3's original buffer)
        Assert::AreEqual(initial_release + 1, g_PoolResourceMonitor.getReleaseCount("test_move_3")); // Check name specific release
        Assert::AreEqual(tm1_count + 1, testManager.getActiveBufferCount()); // One buffer released from manager

        // Test move assignment between different managers (should throw)
        MessageBufferGuard buffer4("test_move_4", testManager2);
        Assert::ExpectException<std::logic_error>([&]() {
            buffer4 = std::move(buffer3); // Move from testManager to testManager2
            });

        // Ensure buffer3 was not affected by the failed move
        Assert::IsNotNull(buffer3.get());
        Assert::AreEqual("Test data", buffer3.get());
        Assert::AreEqual(tm1_count + 1, testManager.getActiveBufferCount());
    }

    TEST_METHOD(TestDetach)
    {
        size_t initial_alloc = g_PoolResourceMonitor.getAllocationCount("test_detach");
        size_t initial_release = g_PoolResourceMonitor.getReleaseCount("test_detach");
        size_t initial_test_manager_count = testManager.getActiveBufferCount();

        char* raw_ptr = nullptr;

        {
            MessageBufferGuard buffer("test_detach", testManager);
            strcpy_s(buffer.get(), TestBufferManager::TEST_BUFFER_SIZE, "Detached data");
            Assert::AreEqual(initial_test_manager_count + 1, testManager.getActiveBufferCount());

            // Detach transfers ownership
            raw_ptr = buffer.detach();

            // Original should be null
            Assert::IsNull(buffer.get());

            // No release should be recorded yet by global monitor
            Assert::AreEqual(initial_release, g_PoolResourceMonitor.getReleaseCount("test_detach"));
            // Buffer still tracked by test manager until explicitly released
            Assert::AreEqual(initial_test_manager_count + 1, testManager.getActiveBufferCount());
        }

        // After scope exit, still no release since we detached
        Assert::AreEqual(initial_release, g_PoolResourceMonitor.getReleaseCount("test_detach"));
        Assert::AreEqual(initial_test_manager_count + 1, testManager.getActiveBufferCount());

        // We can still access the detached buffer
        Assert::AreEqual("Detached data", raw_ptr);

        // Clean up manually using the *same manager* that allocated it
        testManager.releaseBuffer(raw_ptr);

        // Now release count should be up for the manager
        Assert::AreEqual(initial_test_manager_count, testManager.getActiveBufferCount());
        // Detached buffers might not be tracked by the global resource monitor's release count
        // unless the testManager.releaseBuffer also calls g_PoolResourceMonitor.recordRelease.
        // For now, let's assume the global monitor only tracks releases via the guard.
        Assert::AreEqual(initial_release, g_PoolResourceMonitor.getReleaseCount("test_detach"));
    }
};
