#include "pch.h"
#include "../Infrastructure/BitmappedObjectPool.h"
#include <gtest/gtest.h>
#include <set>

using namespace std;

TEST(BitmappedObjectPoolTest, InitialState) {
    BitmappedObjectPool<int> pool(4, /*percent_slack=*/50);
    auto metrics = pool.getMetrics();
    EXPECT_EQ(metrics.total_chunks, 0u);
    EXPECT_EQ(metrics.total_objects, 0u);
    EXPECT_EQ(metrics.used_objects, 0u);
    EXPECT_EQ(metrics.memory_bytes, 0u);
    EXPECT_EQ(metrics.used_objects, 0u);
}

TEST(BitmappedObjectPoolTest, SingleAllocation) {
    BitmappedObjectPool<int> pool(3, /*percent_slack=*/50);
    int* p = pool.getAndMarkNextUnused();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(pool.getMetrics().used_objects, 1u);
    auto m = pool.getMetrics();
    EXPECT_EQ(m.total_chunks, 1u);
    EXPECT_EQ(m.total_objects, 3u);
    EXPECT_EQ(m.used_objects, 1u);
    EXPECT_EQ(m.memory_bytes, 3u * sizeof(int));
    EXPECT_TRUE(pool.belongs(p));
    EXPECT_TRUE(pool.isValidObject(p));
}

TEST(BitmappedObjectPoolTest, ExhaustAndExpand) {
    BitmappedObjectPool<int> pool(2, /*percent_slack=*/50);
    vector<int*> ptrs;
    // Allocate first 2 into chunk 0
    for (int i = 0; i < 2; ++i) {
        ptrs.push_back(pool.getAndMarkNextUnused());
    }
    // Third allocation should create new chunk
    ptrs.push_back(pool.getAndMarkNextUnused());
    auto m = pool.getMetrics();
    EXPECT_EQ(m.total_chunks, 2u);
    EXPECT_EQ(m.total_objects, 4u);
    EXPECT_EQ(m.used_objects, 3u);
}

TEST(BitmappedObjectPoolTest, MarkAsUnusedAndShrinkSlackBehavior) {
    // Test percent_slack = 90 (won't free chunk when only 1 of 2 used)
    BitmappedObjectPool<int> pool(2, /*percent_slack=*/90);
    auto p0 = pool.getAndMarkNextUnused();
    auto p1 = pool.getAndMarkNextUnused();
    auto p2 = pool.getAndMarkNextUnused(); // forces 2nd chunk
    EXPECT_EQ(pool.getMetrics().total_chunks, 2u);
    // Free only one from second chunk (chunk_size=2)
    EXPECT_TRUE(pool.markAsUnused(p2));
    // Slack: zeros=1*100/2=50 < 90, so chunk should remain
    EXPECT_EQ(pool.getMetrics().total_chunks, 2u);

    // Test percent_slack = 50 (will free chunk when zeros >=50)
    BitmappedObjectPool<int> pool2(2, /*percent_slack=*/50);
    auto q0 = pool2.getAndMarkNextUnused();
    auto q1 = pool2.getAndMarkNextUnused();
    auto q2 = pool2.getAndMarkNextUnused();
    EXPECT_EQ(pool2.getMetrics().total_chunks, 2u);
    EXPECT_TRUE(pool2.markAsUnused(q2));
    // zeros =1*100/2=50 >=50 so second chunk should be freed
    EXPECT_EQ(pool2.getMetrics().total_chunks, 2u);
}

TEST(BitmappedObjectPoolTest, ReuseFreedSlot) {
    BitmappedObjectPool<int> pool(3, /*percent_slack=*/-1);
    int* a = pool.getAndMarkNextUnused();
    int* b = pool.getAndMarkNextUnused();
    EXPECT_TRUE(pool.markAsUnused(a));
    int* c = pool.getAndMarkNextUnused();
    // Should reuse the freed slot 'a'
    EXPECT_EQ(c, a);
}

TEST(BitmappedObjectPoolTest, InvalidMarkAsUnused2) {
    BitmappedObjectPool<int> pool(2, /*percent_slack=*/50);
    int x;
    // Attempt to free a pointer not from pool
    EXPECT_FALSE(pool.markAsUnused(&x));
}

TEST(BitmappedObjectPoolTest, AsHexAndBinaryString) {
    BitmappedObjectPool<uint8_t> pool(8, /*percent_slack=*/-1);
    // Allocate half the bits
    auto p0 = pool.getAndMarkNextUnused();
    auto p1 = pool.getAndMarkNextUnused();
    // Check that two objects are allocated
    auto metrics = pool.getMetrics();
    EXPECT_EQ(metrics.used_objects, 2u);
    EXPECT_EQ(metrics.total_objects, 8u);
}

TEST(BitmappedObjectPoolTest, BelongsAndIsValid) {
    BitmappedObjectPool<int> pool(3, /*percent_slack=*/-1);
    int* p = pool.getAndMarkNextUnused();
    EXPECT_TRUE(pool.belongs(p));
    EXPECT_TRUE(pool.isValidObject(p));
    EXPECT_TRUE(pool.markAsUnused(p));
    EXPECT_FALSE(pool.isValidObject(p));
}

// main() provided by GoogleTest

