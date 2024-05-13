/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once
#include <mutex>
#include <vector>
#include "Logger.h"

using namespace std;

template <class T>
class ArrayQueue {
public:
	ArrayQueue(const int size) : data_(size), head_pos_(0), next_pos_(-1) {}
	inline bool isEmpty() const { return next_pos_ == -1; }
	inline bool isFull() const { return next_pos_ == head_pos_; }
	bool enqueue(T&& item);
	bool dequeue(T& item);
	bool peek(T& item, const int item_index = 0);
	bool removeFront(T& item);
	bool removeFront();
	size_t length() const;

private:
	vector<T> data_;
	int head_pos_, next_pos_;
	mutex data_locker_;
};

template<class T>
bool ArrayQueue<T>::enqueue(T&& item)
{
	bool result;
	const std::lock_guard<std::mutex> lock(data_locker_);
	int initial_head = head_pos_;
	int initial_next = next_pos_;
	if (isFull()) {
		Logger::critical("ArrayQueue::enqueue() Queue Full\n");
		result = false;
	}
	else {
		if (next_pos_ == -1) {
			next_pos_ = head_pos_;
		}
		data_[next_pos_] = item;
		next_pos_ = (next_pos_ + 1) % data_.size();
		result = true;
	}
	Logger::debug2("ArrayQueue::enqueue() success initial head_pos_==%d "
		"next_pos_==%d, finish head_pos==%d next_pos==%d\n", 
		initial_head, initial_next, head_pos_, next_pos_);
	return result;
}

template<class T>
bool ArrayQueue<T>::dequeue(T& item) {
	int initial_head = head_pos_;
	int initial_next = next_pos_;
	if (isEmpty()) {
		Logger::debug("ArrayQueue::dequeue() can't, queue is empty\n");
		return false;
	}
	data_locker_.lock();
	item = data_[head_pos_];
	head_pos_ = (head_pos_ + 1) % data_.size();
	if (head_pos_ == next_pos_) {
		next_pos_ = -1;
	}
	data_locker_.unlock();
	Logger::debug2("ArrayQueue::dequeue() success initial head_pos_==%d "
		"next_pos_==%d, finish head_pos==%d next_pos==%d\n", 
		initial_head, initial_next, head_pos_, next_pos_);
	return true;
}

template<class T>
bool ArrayQueue<T>::peek(T& item, const int item_index) {
	if (isEmpty()) {
		return false;
	}
	item = data_[(head_pos_ + item_index) % data_.size()];
	return true;
}

template<class T>
bool ArrayQueue<T>::removeFront(T& item) {
	if (isEmpty()) {
		Logger::debug("ArrayQueue::removeFront(item) can't, queue is empty\n");
		return false;
	}
	int initial_head = head_pos_;
	int initial_next = next_pos_;
	bool result;
	data_locker_.lock();
	// use exact equality rather than logical equality
	if (memcmp(&item, &data_[head_pos_], sizeof(item)) == 0) {
		head_pos_ = (head_pos_ + 1) % data_.size();
		if (head_pos_ == next_pos_) {
			next_pos_ = -1;
		}
		result = true;
	}
	else {
		result = false;
	}
	data_locker_.unlock();
	Logger::debug2("ArrayQueue::removeFront(item) success initial head_pos_==%d "
		"next_pos_==%d, finish head_pos==%d next_pos==%d\n", 
		initial_head, initial_next, head_pos_, next_pos_);
	return result;
}

template<class T>
bool ArrayQueue<T>::removeFront() {
	if (isEmpty()) {
		Logger::debug("ArrayQueue::removeFront() can't, queue is empty\n");
		return false;
	}
	int initial_head = head_pos_;
	int initial_next = next_pos_;
	data_locker_.lock();
	head_pos_ = (head_pos_ + 1) % data_.size();
	if (head_pos_ == next_pos_) {
		next_pos_ = -1;
	}
	data_locker_.unlock();
	Logger::debug2("ArrayQueue::removeFront() success initial head_pos_==%d "
		"next_pos_==%d, finish head_pos==%d next_pos==%d\n",
		initial_head, initial_next, head_pos_, next_pos_);
	return true;
}

template<class T>
size_t ArrayQueue<T>::length() const {
	if (next_pos_ == -1) {
		return 0;
	}
	if (head_pos_ >= next_pos_) {
		return data_.size() - head_pos_ + next_pos_;
	}
	else {
		return next_pos_ - head_pos_;
	}
}

