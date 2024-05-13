/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once
#include "Bitmap.h"
#include <mutex>
#include <string>
#include <vector>

template <class T>
class BitmappedObjectPool
{
public:

	/* Note:	memory is allocated in chunks of multiples of the given template unit.
				percent_slack is used so that for a given chunk, if the chunks above it
				are empty/currently-unused then those extra chunks are available to be
				freed to release the memory.  0% slack means release extra chunks as
				soon as they are unneeded.  100% slack means wait until the current
				chunk is entirely unused before getting rid of ones above it. -1
				percent_slack means never free up chunks, keep them reserved forever. */

	BitmappedObjectPool(const int chunk_size, const int percent_slack) 
		: chunk_size_(chunk_size), percent_slack_(percent_slack) {
	}

	template <class U> BitmappedObjectPool(const BitmappedObjectPool<U>& old_obj) {
		usage_bitmaps_.reserve(old_obj.usage_bitmaps_.size());
		for (const auto& e : old_obj.usage_bitmaps_) {
			usage_bitmaps_.push_back(std::make_shared<Bitmap>(*e));
		}
		data_elements_.reserve(old_obj.data_elements_.size());
		for (const auto& e : old_obj.data_elements_) {
			data_elements_.push_back(std::make_shared<T[]>(*e));
		}
		chunk_size_ = old_obj.chunk_size_;
		percent_slack_ = old_obj.percent_slack_;
	}

	T* getAndMarkNextUnused() {
		// no maximum, if you do too much you'll get an out-of-memory error
		const std::lock_guard<std::mutex> lock(in_use_);
		int32_t bitmap_index = -1;
		int bitnum = -1;
		for (unsigned int i = 0; i < usage_bitmaps_.size(); ++i) {
			int idx = usage_bitmaps_[i]->getAndSetFirstZero();
			if (idx >= 0) {
				bitmap_index = i;
				bitnum = idx;
				break;
			}
		}
		if (bitmap_index == -1) {
			// we didn't find a zero anywhere, so add another chunk
			usage_bitmaps_.push_back(std::make_unique<Bitmap>(chunk_size_, 0));
			data_elements_.push_back(std::make_unique<T[]>(chunk_size_));
			auto first_addr = &data_elements_[data_elements_.size() - 1][0];
			auto last_addr = &data_elements_[data_elements_.size() - 1][chunk_size_ - 1];
			auto difference = last_addr - first_addr;
			bitmap_index = static_cast<int32_t>(usage_bitmaps_.end() - usage_bitmaps_.begin()) - 1;
			bitnum = usage_bitmaps_.back()->getAndSetFirstZero();
		}
		auto& last_data_element = data_elements_[bitmap_index];
		return &last_data_element[bitnum];
	}

	bool markAsUnused(T*& now_unused) {
		const std::lock_guard<std::mutex> lock(in_use_);
		unsigned int now_unused_bitmap_idx = 0;
		uint64_t now_unused_bit = -1;
		auto now_unused_address = now_unused;
		bool found = false;
		for (unsigned int i = 0; !found && i < usage_bitmaps_.size(); ++i) {
			auto start_address = &data_elements_[i][0];
			auto end_address = &data_elements_[i][chunk_size_ - 1];
			if (now_unused_address >= start_address 
				&& now_unused_address <= end_address) {
				now_unused_bitmap_idx = i;
				now_unused_bit = now_unused - start_address;
				found = true;
				// we've found the chunk containing our now_unused
			}
		}
		if (!found) {
			return false;
		}

		if (percent_slack_ != -1) {
			if (now_unused_bitmap_idx < usage_bitmaps_.size() - 1) {
				// we're below the maximum chunk
				// is every one above us empty?
				bool empty_above_us = true;
				for (unsigned int cn = now_unused_bitmap_idx + 1; 
					cn < usage_bitmaps_.size(); ++cn) {
					if (usage_bitmaps_[cn]->countOnes() > 0) {
						empty_above_us = false;
						break;
					}
				}
				// if empty above us then see if we have enough slack
				if (empty_above_us) {
					int number_of_zeroes 
						= usage_bitmaps_[now_unused_bitmap_idx]->countZeroes();
					if (number_of_zeroes * 100 / chunk_size_ >= percent_slack_) {
						// if empty above us and we have enough slack, 
						// get rid of extra chunk(s)
						usage_bitmaps_.resize(now_unused_bitmap_idx + 1);
						data_elements_.resize(now_unused_bitmap_idx + 1);
					}
				}
			}
		}
		usage_bitmaps_[now_unused_bitmap_idx]->setBitTo((int) now_unused_bit, 0);
		return true;
	}

	int countBuffers() const {
		int count = 0;
		for (auto& bm : usage_bitmaps_) {
			count += bm->countOnes();
		}
		return count;
	}

	const std::string asHexString() const {
		std::string result;
		for (auto& bm : usage_bitmaps_) {
			result.append(bm->asHexString());
		}
		return result;
	}

	const std::string asBinaryString() const {
		std::string result;
		for (auto& bm : usage_bitmaps_) {
			result.append(bm->asBinaryString());
		}
		return result;
	}

private:
	std::mutex in_use_;
	std::vector<std::shared_ptr<Bitmap>> usage_bitmaps_;
	std::vector<std::shared_ptr<T[]>> data_elements_;
	int chunk_size_;
	int percent_slack_;
};

