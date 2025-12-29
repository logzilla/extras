#include "pch.h"
#include "CppUnitTest.h"
#include "../AgentLib/MessageBufferGuard.h"
#include "../AgentLib/MessageQueue.h"
#include "../Infrastructure/PoolResourceMonitor.h"
#include <memory>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Syslog_agent;

namespace InfrastructureTest {

	TEST_CLASS(MessageGuardTests) {
	public:
		
		TEST_METHOD(MessageBufferGuard_AutoRelease) {
			// Create a buffer pool
			auto bufferPool = std::make_unique<BitmappedObjectPool<MessageQueue::MessageBuffer>>(10, 0);
			
			// Initial state
			int initialCount = bufferPool->countBuffers();
			
			{
				// Create a guard and allocate a buffer
				MessageBufferGuard guard("test_buffer", *bufferPool);
				
				// Should have allocated one buffer
				Assert::IsNotNull(guard.get());
				Assert::AreEqual(initialCount + 1, bufferPool->countBuffers());
				
				// Use the buffer
				MessageQueue::MessageBuffer* buffer = guard.get();
				strcpy_s(buffer->buffer, MessageQueue::MESSAGE_BUFFER_SIZE, "Test message");
				
				// Buffer should be valid during this scope
				Assert::AreEqual("Test message", buffer->buffer);
			}
			
			// After scope exit, the buffer should be released back to the pool
			Assert::AreEqual(initialCount, bufferPool->countBuffers());
		}
		
		TEST_METHOD(MessageGuard_AutoRelease) {
			// Create a message pool
			auto messagePool = std::make_unique<BitmappedObjectPool<MessageQueue::Message>>(10, 0);
			
			// Initial state
			int initialCount = messagePool->countBuffers();
			
			{
				// Create a guard and allocate a message
				MessageGuard guard("test_message", *messagePool);
				
				// Should have allocated one message
				Assert::IsNotNull(guard.get());
				Assert::AreEqual(initialCount + 1, messagePool->countBuffers());
				
				// Use the message
				MessageQueue::Message* msg = guard.get();
				msg->data_length = 100;
				
				// Message should be valid during this scope
				Assert::AreEqual(100u, msg->data_length);
			}
			
			// After scope exit, the message should be released back to the pool
			Assert::AreEqual(initialCount, messagePool->countBuffers());
		}
		
		TEST_METHOD(MessageBufferGuard_Detach) {
			// Create a buffer pool
			auto bufferPool = std::make_unique<BitmappedObjectPool<MessageQueue::MessageBuffer>>(10, 0);
			
			// Initial state
			int initialCount = bufferPool->countBuffers();
			
			MessageQueue::MessageBuffer* detachedBuffer = nullptr;
			
			{
				// Create a guard and allocate a buffer
				MessageBufferGuard guard("test_buffer", *bufferPool);
				
				// Should have allocated one buffer
				Assert::AreEqual(initialCount + 1, bufferPool->countBuffers());
				
				// Detach the buffer (transfers ownership)
				detachedBuffer = guard.detach();
				Assert::IsNotNull(detachedBuffer);
			}
			
			// After scope exit, buffer should still be allocated since it was detached
			Assert::AreEqual(initialCount + 1, bufferPool->countBuffers());
			
			// Clean up manually
			bufferPool->markAsUnused(detachedBuffer);
			Assert::AreEqual(initialCount, bufferPool->countBuffers());
		}
	};
} 