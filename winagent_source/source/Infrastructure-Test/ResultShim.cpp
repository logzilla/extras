#include "pch.h"
#include "../Infrastructure/Result.h"

// This file ensures that the Result class is properly linked in the test project
// by providing an explicit reference to its vtable.

// Force the compiler to instantiate the Result class vtable in this compilation unit
namespace {
    // Dummy function that uses the Result class to force instantiation of its vtable
    void EnsureResultVtableIsLinked() {
        Syslog_agent::Result result;
        const char* what = result.what();
        bool success = result.isSuccess();
        (void)what;  // Prevent unused variable warning
        (void)success;  // Prevent unused variable warning
    }
} 