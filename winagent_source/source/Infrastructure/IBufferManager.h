#pragma once

/**
 * @brief Interface for managing the allocation and release of character buffers.
 *
 * This allows components like MessageBufferGuard to work with different
 * underlying buffer sources (e.g., global pools, test allocators).
 */
struct IBufferManager {
    virtual ~IBufferManager() = default;

    /**
     * @brief Allocates a character buffer.
     * @param name An optional identifier for the buffer's purpose (for monitoring/debugging).
     * @return A pointer to the allocated buffer, or nullptr on failure.
     */
    virtual char* allocateBuffer(const char* name) = 0;

    /**
     * @brief Releases a previously allocated buffer.
     * @param buffer A pointer to the buffer obtained from allocateBuffer.
     */
    virtual void releaseBuffer(char* buffer) = 0;
}; 