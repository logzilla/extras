/*  -------------------------------------------------------------------------
    BitmappedObjectPoolImpl – template inline section separated to reduce
    compile‑time coupling.  Requires the full definition of ResourceMonitor.
    -------------------------------------------------------------------------*/

    #pragma once

    #include "BitmappedObjectPool.h"
    #include "ResourceMonitor.h"
    
    //  ResourceMonitor is assumed to expose the following two member functions:
    //      void* resourceConsumed(int currentCount, int totalCapacity);
    //      void* resourceReturned(int currentCount, int totalCapacity);
    //  They can return an optional token (ignored here) or nullptr.
    
    // ---------------------------------------------------------------------------
    //  setResourceMonitor – installs/remove hooks in a threadsafe way
    // ---------------------------------------------------------------------------
    
    template <class T>
    void BitmappedObjectPool<T>::setResourceMonitor(ResourceMonitor* monitor)
    {
        if (!monitor) { clearMonitoring(); return; }
    
        setMonitoring(
            static_cast<void*>(monitor),
            [monitor](int current, int capacity) -> void* {
                return monitor->resourceConsumed(current, capacity);
            },
            [monitor](int current, int capacity) -> void* {
                return monitor->resourceReturned(current, capacity);
            });
    }
    