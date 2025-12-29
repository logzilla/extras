#pragma once

#include <string>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <functional>

#ifdef _WIN32
#include <Windows.h>
#endif

// Define export macro for Infrastructure DLL
#ifdef INFRASTRUCTURE_EXPORTS
    #define INFRASTRUCTURE_API __declspec(dllexport)
#else
    #define INFRASTRUCTURE_API __declspec(dllimport)
#endif

namespace Infrastructure {

// Forward declaration
class HandleTracker;

// Global handle tracker instance
INFRASTRUCTURE_API HandleTracker& getHandleTracker();

// Base handle wrapper for cross-platform usage
class HandleWrapper {
public:
    virtual ~HandleWrapper() = default;
    virtual bool isValid() const = 0;
    virtual void close() = 0;
    virtual const char* getTypeName() const = 0;
};

// Platform-agnostic handle tracker
class HandleTracker {
public:
    struct HandleInfo {
        std::string name;
        std::string location;
        std::string type;
        uint32_t thread_id;
        std::chrono::steady_clock::time_point creation_time;
    };

    // Track a generic handle
    template<typename HandleType>
    void trackHandle(HandleType handle, const char* name, const char* location, const char* type) {
        if (!isHandleValid(handle)) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        HandleInfo info;
        info.name = name ? name : "unnamed";
        info.location = location ? location : "unknown";
        info.type = type ? type : "unknown";
#ifdef _WIN32
        info.thread_id = GetCurrentThreadId();
#else
        info.thread_id = 0; // Could use pthread_self() on Linux
#endif
        info.creation_time = std::chrono::steady_clock::now();
        
        // Use pointer value as key
        uintptr_t handleKey = reinterpret_cast<uintptr_t>(handle);
        active_handles_[handleKey] = std::move(info);
    }
    
    // Release a tracked handle
    template<typename HandleType>
    void releaseHandle(HandleType handle) {
        if (!handle) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        uintptr_t handleKey = reinterpret_cast<uintptr_t>(handle);
        active_handles_.erase(handleKey);
    }
    
    // Check if a handle exists in the tracker
    template<typename HandleType>
    bool isHandleTracked(HandleType handle) const {
        if (!handle) return false;
        
        std::lock_guard<std::mutex> lock(mutex_);
        uintptr_t handleKey = reinterpret_cast<uintptr_t>(handle);
        return active_handles_.find(handleKey) != active_handles_.end();
    }
    
    // Report all tracked handles
    void reportActiveHandles() const;
    
private:
    template<typename HandleType>
    bool isHandleValid(HandleType handle) const {
        return handle != nullptr;
    }
    
    mutable std::mutex mutex_;
    std::unordered_map<uintptr_t, HandleInfo> active_handles_;
};

// Generic RAII handle wrapper with customizable cleanup
template<typename HandleType, typename CleanupFunc>
class ScopedHandle {
public:
    ScopedHandle(HandleType& handle, CleanupFunc cleanup, 
                 const char* name = "Handle", 
                 const char* location = __FUNCTION__,
                 const char* type = "Generic")
        : handle_(handle), cleanup_(cleanup), name_(name), type_(type) {
        if (handle) {
            getHandleTracker().trackHandle(handle, name, location, type);
        }
    }
    
    ~ScopedHandle() {
        if (handle_) {
            cleanup_(handle_);
            getHandleTracker().releaseHandle(handle_);
            handle_ = nullptr;
        }
    }
    
    // Prevent copying
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    
private:
    HandleType& handle_;
    CleanupFunc cleanup_;
    const char* name_;
    const char* type_;
};

} // namespace Infrastructure 