#include "pch.h"
#include "../Infrastructure/ArrayQueue.h"
#include <string>
#include <thread>
#include <atomic>
#include <vector>

using namespace std;

class ArrayQueueTest : public ::testing::Test {
protected:
    static constexpr int TEST_QUEUE_SIZE = 10;
    
    void SetUp() override {
        // Create a new queue for each test
        queue = new ArrayQueue<std::string>(TEST_QUEUE_SIZE);
    }
    
    void TearDown() override {
        delete queue;
    }
    
    ArrayQueue<std::string>* queue;
};

// Test constructor with invalid size
TEST_F(ArrayQueueTest, ConstructorInvalidSize) {
    EXPECT_THROW(ArrayQueue<int>(0), std::invalid_argument);
    EXPECT_THROW(ArrayQueue<std::string>(-1), std::invalid_argument);
}

// Test basic operations
TEST_F(ArrayQueueTest, BasicOperations) {
    // Queue should start empty
    EXPECT_TRUE(queue->isEmpty());
    EXPECT_FALSE(queue->isFull());
    EXPECT_EQ(queue->length(), 0);
    
    // Attempt dequeue on empty queue
    std::string item;
    EXPECT_FALSE(queue->dequeue(item));
    
    // Add an item
    EXPECT_TRUE(queue->enqueue(std::string("test1")));
    EXPECT_FALSE(queue->isEmpty());
    EXPECT_EQ(queue->length(), 1);
    
    // Peek at the item
    EXPECT_TRUE(queue->peek(item));
    EXPECT_EQ(item, "test1");
    
    // The queue should still have the item
    EXPECT_EQ(queue->length(), 1);
    
    // Remove the item
    EXPECT_TRUE(queue->removeFront());
    EXPECT_TRUE(queue->isEmpty());
    EXPECT_EQ(queue->length(), 0);
}

// Test filling the queue
TEST_F(ArrayQueueTest, QueueFilling) {
    // Fill the queue
    for (int i = 0; i < TEST_QUEUE_SIZE; i++) {
        EXPECT_TRUE(queue->enqueue(std::string("item") + std::to_string(i)));
    }
    
    // Queue should be full
    EXPECT_FALSE(queue->isEmpty());
    EXPECT_TRUE(queue->isFull());
    EXPECT_EQ(queue->length(), TEST_QUEUE_SIZE);
    
    // Try adding to full queue
    EXPECT_FALSE(queue->enqueue(std::string("toomany")));
    
    // Remove an item
    std::string item;
    EXPECT_TRUE(queue->dequeue(item));
    EXPECT_EQ(item, "item0");
    
    // Queue should no longer be full
    EXPECT_FALSE(queue->isFull());
    
    // Should be able to add again
    EXPECT_TRUE(queue->enqueue(std::string("new")));
}

// Test wrapping behavior
TEST_F(ArrayQueueTest, Wrapping) {
    // Fill queue partly
    for (int i = 0; i < 5; i++) {
        queue->enqueue(std::string("item") + std::to_string(i));
    }
    
    // Remove some items
    std::string item;
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(queue->dequeue(item));
        EXPECT_EQ(item, std::string("item") + std::to_string(i));
    }
    
    // Add more items to cause wrapping
    for (int i = 0; i < 5; i++) {
        queue->enqueue(std::string("wrap") + std::to_string(i));
    }
    
    // Check remaining original items
    for (int i = 3; i < 5; i++) {
        EXPECT_TRUE(queue->dequeue(item));
        EXPECT_EQ(item, std::string("item") + std::to_string(i));
    }
    
    // Check wrapped items
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(queue->dequeue(item));
        EXPECT_EQ(item, std::string("wrap") + std::to_string(i));
    }
    
    // Queue should be empty
    EXPECT_TRUE(queue->isEmpty());
}

// Test peek at different indexes
TEST_F(ArrayQueueTest, PeekAtIndex) {
    // Add several items
    for (int i = 0; i < 5; i++) {
        queue->enqueue(std::string("peek") + std::to_string(i));
    }
    
    // Peek at each position
    std::string item;
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(queue->peek(item, i));
        EXPECT_EQ(item, std::string("peek") + std::to_string(i));
    }
    
    // Peek at invalid index
    EXPECT_FALSE(queue->peek(item, -1));
    EXPECT_FALSE(queue->peek(item, 5));
}

// Test remove specific item
TEST_F(ArrayQueueTest, RemoveSpecific) {
    // Add several items
    for (int i = 0; i < 3; i++) {
        queue->enqueue(std::string("item") + std::to_string(i));
    }
    
    // Try to remove non-matching item
    EXPECT_FALSE(queue->removeFront(std::string("nonexistent")));
    
    // Remove matching item
    EXPECT_TRUE(queue->removeFront(std::string("item0")));
    
    // First item should now be item1
    std::string item;
    EXPECT_TRUE(queue->peek(item));
    EXPECT_EQ(item, "item1");
}

// Test concurrent access
TEST_F(ArrayQueueTest, ConcurrentAccess) {
    // Create threads that add and remove
    std::vector<std::thread> threads;
    std::atomic<int> addCount{0};
    std::atomic<int> removeCount{0};
    
    // Producer function
    auto producer = [this, &addCount]() {
        for (int i = 0; i < 100; i++) {
            std::string item = "thread" + std::to_string(i);
            if (queue->enqueue(std::move(item))) {
                addCount++;
            }
            std::this_thread::yield();
        }
    };
    
    // Consumer function
    auto consumer = [this, &removeCount]() {
        for (int i = 0; i < 100; i++) {
            std::string item;
            if (queue->dequeue(item)) {
                removeCount++;
            }
            std::this_thread::yield();
        }
    };
    
    // Start threads
    threads.push_back(std::thread(producer));
    threads.push_back(std::thread(producer));
    threads.push_back(std::thread(consumer));
    threads.push_back(std::thread(consumer));
    
    // Wait for threads to complete
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify concurrent behavior:
    // 1. Queue length should be between 0 and TEST_QUEUE_SIZE
    EXPECT_GE(queue->length(), 0u);
    EXPECT_LE(queue->length(), TEST_QUEUE_SIZE);
    
    // 2. Total items processed should match
    size_t currentLength = queue->length();
    EXPECT_EQ(addCount, removeCount + currentLength);
    
    // 3. Some items should have been successfully processed
    EXPECT_GT(addCount, 0);
    EXPECT_GT(removeCount, 0);
}
