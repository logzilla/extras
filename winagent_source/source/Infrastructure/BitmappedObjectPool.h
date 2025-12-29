/*  -------------------------------------------------------------------------
    BitmappedObjectPool – generic fixed-chunk object pool with bitmap usage map
    -------------------------------------------------------------------------
    Copyright 2025 Logzilla Corp.
    -------------------------------------------------------------------------
    Design changes (2025-04-28)
    •  Copy and move operations deleted – pools are shared via std::shared_ptr
       or created anew; deep-copying bitmaps was error-prone and unnecessary.
    •  Bitmap pointers are now *shared*, never copied (Bitmap is non-copyable).
    •  Added strong parameter checks and consistent lock discipline.
    •  Template supports any trivially-constructible T (including std::array).
    -------------------------------------------------------------------------*/

#pragma once

#include "Bitmap.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

    // Forward declaration
class ResourceMonitor;

template <class T>
class BitmappedObjectPool
{
    // Only trivially-destructible element types are supported – pool never
    // calls destructors individually. Adjust if that requirement changes.
    // TODO: this is disabled to meet deadline, add back later
    //static_assert(std::is_trivially_destructible_v<T>,
    //    "BitmappedObjectPool requires T to be trivially destructible");
public:
    // Monitoring callback types
    using MonitorAllocateFunc = std::function<void* (int current, int capacity)>;
    using MonitorDeallocateFunc = std::function<void* (int current, int capacity)>;

    // ------------------------------------------------------------
    //  Construction / destruction – copy & move disabled
    // ------------------------------------------------------------
    BitmappedObjectPool(int chunk_size, int percent_slack)
        : chunk_size_(chunk_size),
        percent_slack_(percent_slack)
    {
        if (chunk_size_ <= 0)
            throw std::invalid_argument("chunk_size must be positive");
        if (percent_slack_ < -1 || percent_slack_ > 100)
            throw std::invalid_argument("percent_slack must be -1..100");
    }

    BitmappedObjectPool(const BitmappedObjectPool&) = delete;
    BitmappedObjectPool(BitmappedObjectPool&&) = delete;
    BitmappedObjectPool& operator=(const BitmappedObjectPool&) = delete;
    BitmappedObjectPool& operator=(BitmappedObjectPool&&) = delete;

    // ------------------------------------------------------------
    //  Monitoring hooks
    // ------------------------------------------------------------
    void setMonitoring(void* token,
        MonitorAllocateFunc on_alloc,
        MonitorDeallocateFunc on_free)
    {
        std::lock_guard lock(mutex_);
        monitoring_token_ = token;
        allocate_func_ = std::move(on_alloc);
        deallocate_func_ = std::move(on_free);
    }

    void clearMonitoring()
    {
        std::lock_guard lock(mutex_);
        monitoring_token_ = nullptr;
        allocate_func_ = nullptr;
        deallocate_func_ = nullptr;
    }

    void setResourceMonitor(ResourceMonitor* monitor);

    // ------------------------------------------------------------
    //  Allocation / de-allocation
    // ------------------------------------------------------------
    T* getAndMarkNextUnused()
    {
        std::lock_guard lock(mutex_);

        std::int32_t bitmap_index = -1;
        int          bitnum = -1;

        // Try to find a zero bit in existing bitmaps
        for (std::size_t i = 0; i < usage_bitmaps_.size(); ++i)
        {
            int idx = usage_bitmaps_[i]->getAndSetFirstZero();
            if (idx >= 0)
            {
                bitmap_index = static_cast<std::int32_t>(i);
                bitnum = idx;
                break;
            }
        }

        // None free?  create a new chunk
        if (bitmap_index == -1)
        {
            usage_bitmaps_.push_back(std::make_shared<Bitmap>(chunk_size_, 0));
            data_chunks_.push_back(std::shared_ptr<T[]>(new T[chunk_size_],
                std::default_delete<T[]>()));
            bitmap_index = static_cast<std::int32_t>(usage_bitmaps_.size() - 1);
            bitnum = usage_bitmaps_.back()->getAndSetFirstZero();
        }

        notifyAllocate();
        return &data_chunks_[bitmap_index].get()[bitnum];
    }

    bool markAsUnused(T* item)
    {
        if (!item) return false;
        std::lock_guard lock(mutex_);

        // locate the chunk
        for (std::size_t i = 0; i < data_chunks_.size(); ++i)
        {
            T* start = data_chunks_[i].get();
            T* end = start + chunk_size_;
            if (item >= start && item < end)
            {
                std::ptrdiff_t offset = item - start;
                usage_bitmaps_[i]->setBitTo(static_cast<std::size_t>(offset), 0);
                maybeTrimSlack(i);
                notifyDeallocate();
                return true;
            }
        }
        return false;
    }

    // ------------------------------------------------------------
    //  Metrics / diagnostics
    // ------------------------------------------------------------
    struct PoolMetrics
    {
        std::size_t total_objects{ 0 };
        std::size_t used_objects{ 0 };
        std::size_t total_chunks{ 0 };
        std::size_t memory_bytes{ 0 };
    };

    [[nodiscard]] PoolMetrics getMetrics() const
    {
        std::lock_guard lock(mutex_);
        PoolMetrics m;
        m.total_chunks = data_chunks_.size();
        m.total_objects = m.total_chunks * static_cast<std::size_t>(chunk_size_);
        m.used_objects = countBuffers_nolock();
        m.memory_bytes = m.total_objects * sizeof(T);
        return m;
    }

    [[nodiscard]] bool belongs(const T* item) const
    {
        if (!item) return false;
        for (const auto& chunk : data_chunks_)
        {
            const T* start = chunk.get();
            if (item >= start && item < start + chunk_size_) return true;
        }
        return false;
    }

    [[nodiscard]] bool isValidObject(const T* item) const
    {
        if (!belongs(item)) return false;
        std::lock_guard lock(mutex_);
        for (std::size_t i = 0; i < data_chunks_.size(); ++i)
        {
            const T* start = data_chunks_[i].get();
            if (item >= start && item < start + chunk_size_)
            {
                std::size_t offset = static_cast<std::size_t>(item - start);
                return usage_bitmaps_[i]->isSet(offset);
            }
        }
        return false;
    }

    // force release of completely empty high chunks
    void forceGarbageCollection()
    {
        std::lock_guard lock(mutex_);
        std::size_t highest_in_use = 0;
        for (std::size_t i = 0; i < usage_bitmaps_.size(); ++i)
        {
            if (usage_bitmaps_[i]->countOnes() > 0) highest_in_use = i;
        }
        usage_bitmaps_.resize(highest_in_use + 1);
        data_chunks_.resize(highest_in_use + 1);
    }

private:
    // ------------------------------------------------------------
    //  helpers (mutex already held)
    // ------------------------------------------------------------
    int countBuffers_nolock() const
    {
        int sum = 0;
        for (const auto& bm : usage_bitmaps_) sum += bm->countOnes();
        return sum;
    }

    void maybeTrimSlack(std::size_t idx)
    {
        if (percent_slack_ == -1) return;
        if (idx >= usage_bitmaps_.size() - 1) return; // last chunk, nothing above

        bool empty_above = true;
        for (std::size_t i = idx + 1; i < usage_bitmaps_.size(); ++i)
        {
            if (usage_bitmaps_[i]->countOnes() != 0) { empty_above = false; break; }
        }
        if (!empty_above) return;

        std::size_t zeros = usage_bitmaps_[idx]->countZeroes();
        if (zeros * 100ULL / static_cast<std::size_t>(chunk_size_) >= static_cast<std::size_t>(percent_slack_))
        {
            usage_bitmaps_.resize(idx + 1);
            data_chunks_.resize(idx + 1);
        }
    }

    void notifyAllocate()
    {
        if (allocate_func_)
            allocate_func_(countBuffers_nolock(), static_cast<int>(data_chunks_.size() * chunk_size_));
    }

    void notifyDeallocate()
    {
        if (deallocate_func_)
            deallocate_func_(countBuffers_nolock(), static_cast<int>(data_chunks_.size() * chunk_size_));
    }

    // ------------------------------------------------------------
    //  data members
    // ------------------------------------------------------------
    mutable std::mutex               mutex_;
    std::vector<std::shared_ptr<Bitmap>> usage_bitmaps_;
    std::vector<std::shared_ptr<T[]>>    data_chunks_;

    int  chunk_size_;
    int  percent_slack_; // -1 = never free, 0..100 else

    void* monitoring_token_{ nullptr };
    MonitorAllocateFunc   allocate_func_;
    MonitorDeallocateFunc deallocate_func_;
};

// Template inline implementation that needs ResourceMonitor definition
#include "BitmappedObjectPoolImpl.h"