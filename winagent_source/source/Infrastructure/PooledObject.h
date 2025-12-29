#pragma once

#include "BitmappedObjectPool.h"
#include <utility>

/**
    * RAII wrapper for objects obtained from a BitmappedObjectPool.
    * Automatically returns the object to the pool when going out of scope.
    */
template <typename T>
class PooledObject {
private:
    T* ptr_;
    BitmappedObjectPool<T>& pool_;
    bool released_;

public:
    PooledObject(T* ptr, BitmappedObjectPool<T>& pool) 
        : ptr_(ptr), pool_(pool), released_(false) {}
        
    ~PooledObject() {
        release();
    }
        
    // Prevent copying
    PooledObject(const PooledObject&) = delete;
    PooledObject& operator=(const PooledObject&) = delete;
        
    // Allow moving
    PooledObject(PooledObject&& other) noexcept 
        : ptr_(other.ptr_), pool_(other.pool_), released_(other.released_) {
        other.ptr_ = nullptr;
        other.released_ = true;
    }
        
    PooledObject& operator=(PooledObject&& other) noexcept {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            released_ = other.released_;
            other.ptr_ = nullptr;
            other.released_ = true;
        }
        return *this;
    }
        
    void release() {
        if (!released_ && ptr_) {
            pool_.markAsUnused(ptr_);
            released_ = true;
            ptr_ = nullptr;
        }
    }
        
    T* get() const { return ptr_; }
    T* operator->() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    operator T*() const { return ptr_; }
        
    // Detach the pointer from this RAII object without releasing it
    T* detach() {
        released_ = true;
        T* temp = ptr_;
        ptr_ = nullptr;
        return temp;
    }
};

/**
    * Helper function to create a PooledObject from a pool.
    */
template <typename T>
PooledObject<T> makePooled(BitmappedObjectPool<T>& pool) {
    return PooledObject<T>(pool.getAndMarkNextUnused(), pool);
}
