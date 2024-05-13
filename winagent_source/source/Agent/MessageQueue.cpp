/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#include "stdafx.h"
#include "MessageQueue.h"


MessageQueue::MessageQueue(
	uint32_t message_queue_size,
	uint32_t message_buffers_chunk_size
) :
	message_queue_size_(message_queue_size),
	message_queue_chunk_size_(message_buffers_chunk_size),
	in_use_counter_(0)
{
	send_buffers_queue_ = make_unique<ArrayQueue<Message>>(message_queue_size_);
	send_buffers_ = make_unique<BitmappedObjectPool<MessageBuffer>>(message_buffers_chunk_size, MESSAGE_QUEUE_SLACK_PERCENT);
}


bool MessageQueue::enqueue(const char* message_content, const uint32_t message_len) {
	if (isFull()) {
		return false;
	}
	
	std::unique_lock<std::recursive_mutex> lock(queue_mutex_, std::defer_lock);

	while (true) {
		lock.lock();
		if (!isFull()) {
			break;
		}
		lock.unlock();
		notify_cv_.wait(lock); // Wait for notification
	}

	Message message;
	ZeroMemory(&message, sizeof(message));
	int leftover = message_len % MESSAGE_BUFFER_SIZE;
	int num_chunks = message_len / MESSAGE_BUFFER_SIZE + (leftover == 0 ? 0 : 1);
	char* bufptr = const_cast<char*>(message_content);
	message.data_length = message_len;
	for (int c = 0; c < num_chunks; ++c) {
		MessageBuffer* buffer = send_buffers_->getAndMarkNextUnused();
		int buffer_size = (leftover == 0 || c < num_chunks - 1) ? MESSAGE_BUFFER_SIZE : leftover;
		memcpy(buffer->buffer, bufptr, buffer_size);
		message.message_buffers[c] = buffer;
		bufptr += MESSAGE_BUFFER_SIZE;
	}

	bool result = send_buffers_queue_->enqueue(std::move(message));
	return result;
}

int MessageQueue::dequeue(char* message_content, const uint32_t max_len) {
	std::unique_lock<std::recursive_mutex> lock(queue_mutex_);

	if (send_buffers_queue_->isEmpty()) {
		return -1;  // Early exit if the queue is empty
	}

	Message message;
	if (!send_buffers_queue_->peek(message)) {
		Logger::critical("MessageQueue::dequeue() : could not peek queue\n");
		return -1;
	}

	if (message.data_length > max_len) {
		Logger::critical("MessageQueue::dequeue() : message data size %d exceeds dequeue size %d\n", message.data_length, max_len);
		return -1;
	}

	if (!send_buffers_queue_->removeFront()) {
		Logger::critical("MessageQueue::dequeue() : could not release peeked message\n");
		return -1;
	}

	uint32_t total_copied = 0;
	for (uint32_t i = 0; i < message.data_length && total_copied < max_len; i += MESSAGE_BUFFER_SIZE) {
		uint32_t chunk_size = message.data_length - total_copied;
		if (MESSAGE_BUFFER_SIZE < chunk_size) {
			chunk_size = MESSAGE_BUFFER_SIZE;
		}
		memcpy(message_content + total_copied, message.message_buffers[i / MESSAGE_BUFFER_SIZE]->buffer, chunk_size);
		send_buffers_->markAsUnused(message.message_buffers[i / MESSAGE_BUFFER_SIZE]);
		total_copied += chunk_size;
	}

	notify_cv_.notify_one();
	return total_copied;
}



int MessageQueue::peek(char* message_content, const uint32_t max_len, 
	const uint32_t item_index) const {
	Message message;
	bool error = false;
	if (send_buffers_queue_->isEmpty()) {
		return -1;
	}
	if (!send_buffers_queue_->peek(message, item_index)) {
		Logger::critical("MessageQueue::dequeue() : could not peek queue\n");
		error = true;
	}
	if (!error && message.data_length > max_len) {
		Logger::critical("MessageQueue::dequeue() : message data size %d exceeds "
			"dequeue size %d\n", message.data_length, max_len);
		error = true;
	}
	if (error) {
		return -1;
	}
	int leftover = message.data_length % MESSAGE_BUFFER_SIZE;
	int num_chunks = message.data_length / MESSAGE_BUFFER_SIZE + (leftover == 0 ? 0 : 1);
	char* bufptr = const_cast<char*>(message_content);
	for (int c = 0; c < num_chunks; ++c) {
		int copy_size = (leftover == 0 || c < num_chunks - 1) ? MESSAGE_BUFFER_SIZE : leftover;
		memcpy(bufptr, message.message_buffers[c]->buffer, copy_size);
		bufptr += MESSAGE_BUFFER_SIZE;
	}
	return message.data_length;

}


bool MessageQueue::removeFront() {
	Message message;
	if (send_buffers_queue_->isEmpty()) {
		return false;
	}
	bool was_error = false;
	std::unique_lock<std::recursive_mutex> lock(queue_mutex_);
	if (!send_buffers_queue_->peek(message)) {
		Logger::critical("MessageQueue::dequeue() : could not peek queue\n");
		was_error = true;
	}
	else {
		send_buffers_queue_->removeFront();
		int leftover = message.data_length % MESSAGE_BUFFER_SIZE;
		int num_chunks = message.data_length / MESSAGE_BUFFER_SIZE + (leftover == 0 ? 0 : 1);
		for (int c = 0; c < num_chunks; ++c) {
			send_buffers_->markAsUnused(message.message_buffers[c]);
		}
	}
	if (was_error) {
		return false;
	}
	return true;
}

