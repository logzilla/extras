#include "pch.h"
#include "../Infrastructure/BitmappedObjectPool.h"
#include <vector>
#include <cstdint>

using namespace std;

TEST(BitmappedObjectPoolTest, AllocationAndRecycling) {
    // Use a simple type (e.g., int) for the object pool.
    // Create a pool with a chunk size of 10 elements and 50% slack.
    BitmappedObjectPool<int> pool(10, 50);

    vector<int*> allocated;

    // Allocate 15 objects (forcing the pool to add more than one chunk).
    for (int i = 0; i < 15; ++i) {
        int* obj = pool.getAndMarkNextUnused();
        ASSERT_NE(obj, nullptr);
        *obj = i;
        allocated.push_back(obj);
    }

    // Verify that the pool reports 15 allocated objects.
    EXPECT_EQ(pool.getMetrics().used_objects, 15u);

    // Check that belongs and isValidObject work as expected.
    for (int i = 0; i < 15; ++i) {
        EXPECT_TRUE(pool.belongs(allocated[i]));
        EXPECT_TRUE(pool.isValidObject(allocated[i]));
    }

    // Mark the first 5 objects as unused.
    for (int i = 0; i < 5; ++i) {
        int* obj = allocated[i];
        bool success = pool.markAsUnused(obj);
        EXPECT_TRUE(success);
        // After marking as unused, isValidObject should return false.
        EXPECT_FALSE(pool.isValidObject(obj));
    }

    // The total count of allocated objects should now be 10.
    EXPECT_EQ(pool.getMetrics().used_objects, 10u);
}

TEST(BitmappedObjectPoolTest, InvalidMarkAsUnused) {
    BitmappedObjectPool<int> pool(10, 50);
    int dummy = 42;
    // Since 'dummy' was never allocated from the pool, marking it as unused should fail.
    EXPECT_FALSE(pool.markAsUnused(&dummy));
}
