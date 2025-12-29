#include "pch.h"
#include "SlidingWindowMetrics.h"

#if ONLY_FOR_DEBUGGING_CURRENTLY_DISABLED

SlidingWindowMetrics& SlidingWindowMetrics::instance() {
    static SlidingWindowMetrics instance;
    return instance;
}

#endif
