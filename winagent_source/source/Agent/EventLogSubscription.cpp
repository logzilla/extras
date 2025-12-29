#include "stdafx.h"
#include "EventLogSubscription.h"
#include "Logger.h"
#include "../AgentLib/Registry.h"
#include "SlidingWindowMetrics.h"
#include "Util.h"

#pragma comment(lib, "wevtapi.lib")

namespace Syslog_agent {

    // C-style function for handling SEH exceptions
    // This has no C++ objects so it can safely use __try/__except
    static DWORD HandleSubscriptionSEH(
        EVT_SUBSCRIBE_NOTIFY_ACTION action,
        PVOID pContext,
        EVT_HANDLE hEvent)
    {
        auto logger = LOG_THIS;

        __try {
            // Call into the C++ implementation
            auto subscription = reinterpret_cast<EventLogSubscription*>(pContext);
            if (!subscription) {
                return ERROR_INVALID_PARAMETER;
            }

            // Handle updates to bookmark - this might throw SEH
            if (action == EvtSubscribeActionDeliver && hEvent) {
                EVT_HANDLE bookmark = subscription->getBookmark();
                if (bookmark) {
                    if (!subscription->updateBookmark(hEvent)) {
                        DWORD lastError = GetLastError();
                        logger->recoverable_error("HandleSubscriptionSEH()> Failed to update bookmark, error: %lu\n",
                            lastError);
                        return lastError;
                    }
                    return ERROR_SUCCESS;
                }
            }

            return ERROR_SUCCESS; // Successfully handled SEH-prone operations
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            // Return the Windows exception code
            DWORD exceptionCode = GetExceptionCode();
            logger->critical("HandleSubscriptionSEH()> Structured exception: 0x%08X at %s:%d\n",
                exceptionCode, __FILE__, __LINE__);
            return exceptionCode;
        }
    }

    // Move constructor
    EventLogSubscription::EventLogSubscription(EventLogSubscription&& source) noexcept
        : subscription_name_(std::move(source.subscription_name_)),
        channel_(std::move(source.channel_)),
        query_(std::move(source.query_)),
        event_handler_(std::move(source.event_handler_)),
        bookmark_(source.bookmark_),
		only_while_running_(source.only_while_running_),
        subscription_handle_(source.subscription_handle_),
        subscription_active_(false),
        bookmark_modified_(source.bookmark_modified_),
        last_bookmark_save_(source.last_bookmark_save_),
        events_since_last_save_(0)
    {
        memcpy(bookmark_xml_buffer_, source.bookmark_xml_buffer_, sizeof(bookmark_xml_buffer_));

        // Clear source pointers to prevent double deletion
        source.bookmark_ = NULL;
        source.subscription_handle_ = NULL;
        source.bookmark_xml_buffer_[0] = 0;
        // No need to reset event_handler_ as it's been moved

        if (source.subscription_active_) {
            source.subscription_active_ = false;
            subscribe(bookmark_xml_buffer_, source.only_while_running_);
        }
    }

    // Move assignment operator
    EventLogSubscription& EventLogSubscription::operator=(EventLogSubscription&& source) noexcept {
        if (this != &source) {
            // Clean up existing resources
            if (subscription_active_) {
                cancelSubscription();
            }
            
            if (bookmark_ != NULL) {
                EvtClose(bookmark_);
                bookmark_ = NULL;
            }
            
            // Move resource ownership
            subscription_name_ = std::move(source.subscription_name_);
            channel_ = std::move(source.channel_);
            query_ = std::move(source.query_);
            event_handler_ = std::move(source.event_handler_);
            bookmark_ = source.bookmark_;
            only_while_running_ = source.only_while_running_;
            subscription_handle_ = source.subscription_handle_;
            subscription_active_ = false; // We'll resubscribe if needed
            bookmark_modified_ = source.bookmark_modified_;
            last_bookmark_save_ = source.last_bookmark_save_;
            events_since_last_save_ = 0;
            
            memcpy(bookmark_xml_buffer_, source.bookmark_xml_buffer_, sizeof(bookmark_xml_buffer_));
            
            // Clear source handles to prevent double deletion
            source.bookmark_ = NULL;
            source.subscription_handle_ = NULL;
            source.bookmark_xml_buffer_[0] = 0;
            // event_handler_ is already moved, no need to clear
            
            // Resubscribe if the source was active
            if (source.subscription_active_) {
                source.subscription_active_ = false;
                subscribe(bookmark_xml_buffer_, source.only_while_running_);
            }
        }
        
        return *this;
    }

    // Destructor
    EventLogSubscription::~EventLogSubscription() {
        auto logger = LOG_THIS;
        
        try {
            if (subscription_active_) {
                cancelSubscription();
            }
            
            if (bookmark_) {
                if (!EvtClose(bookmark_)) {
                    DWORD errorCode = GetLastError();
                    logger->recoverable_error("EventLogSubscription::~EventLogSubscription()> Failed to close bookmark handle (error %d)\n", 
                        errorCode);
                    // Continue despite error - we've logged it
                }
                bookmark_ = NULL;
            }
        } catch (const std::exception& e) {
            logger->critical("EventLogSubscription::~EventLogSubscription()> Exception during cleanup: %s\n", e.what());
            // Ensure destructor doesn't throw
        } catch (...) {
            logger->critical("EventLogSubscription::~EventLogSubscription()> Unknown exception during cleanup\n");
            // Ensure destructor doesn't throw
        }
    }

    void EventLogSubscription::subscribe(const wstring& bookmark_xml, const bool only_while_running) {
        auto logger = LOG_THIS;
        char channel_buf[1024];
        Util::wstr2str(channel_buf, sizeof(channel_buf), channel_.c_str());
        logger->debug("EventLogSubscription::subscribe()> Subscribing to %s\n", channel_buf);

        if (subscription_active_) {
            logger->recoverable_error("EventLogSubscription::subscribe()> subscription already active\n");
            return;
        }

        // Replace unsafe memcpy
        // memcpy(bookmark_xml_buffer_, bookmark_xml.c_str(), sizeof(bookmark_xml_buffer_));
        // Use wcscpy_s for safe wide character string copying
        wcscpy_s(bookmark_xml_buffer_, MAX_BOOKMARK_SIZE, bookmark_xml.c_str());

        only_while_running_ = only_while_running;
        if (bookmark_) {
            EvtClose(bookmark_);
            bookmark_ = NULL;
        }

        EVT_SUBSCRIBE_FLAGS flags;
        EVT_HANDLE subscribe_bookmark = NULL;  // Separate bookmark for subscription

        if (!only_while_running) {
            if (bookmark_xml_buffer_[0] == 0) {
                flags = EvtSubscribeStartAtOldestRecord;
                bookmark_ = EvtCreateBookmark(NULL);  // Create new empty bookmark for tracking
                if (bookmark_ == NULL) {
                    auto error = GetLastError();
                    logger->recoverable_error("EventLogSubscription::subscribe()> Failed to create empty bookmark (error %d)\n", error);
                    return;
                }
                logger->debug2("EventLogSubscription::subscribe()> Created new empty bookmark %p for %s\n", 
                    bookmark_, channel_buf);
                logger->debug("EventLogSubscription::subscribe()> Catch-up mode: subscribing to all events from start for %s\n", 
                    channel_buf);
            } else {
                flags = EvtSubscribeStartAfterBookmark;
                bookmark_ = EvtCreateBookmark(bookmark_xml_buffer_);
                if (bookmark_ == NULL) {
                    auto error = GetLastError();
                    logger->warning("EventLogSubscription::subscribe()> Failed to create bookmark for %s (error %d), falling back to all events from start\n",
                        channel_buf, error);
                    flags = EvtSubscribeStartAtOldestRecord;
                    bookmark_ = EvtCreateBookmark(NULL);  // Create new empty bookmark for tracking
                    if (bookmark_ == NULL) {
                        auto error = GetLastError();
                        logger->recoverable_error("EventLogSubscription::subscribe()> Failed to create empty bookmark (error %d)\n", error);
                        return;
                    }
                    logger->debug2("EventLogSubscription::subscribe()> Created new empty bookmark %p for %s after bookmark load failed\n", 
                        bookmark_, channel_buf);
                } else {
                    logger->debug2("EventLogSubscription::subscribe()> Created bookmark %p from XML for %s\n", 
                        bookmark_, channel_buf);
                    logger->debug("EventLogSubscription::subscribe()> Catch-up mode: Using bookmark for %s\n", channel_buf);
                    subscribe_bookmark = bookmark_;  // Use existing bookmark for subscription
                }
            }
        } else {
            // Future-only mode
            flags = EvtSubscribeToFutureEvents;
            bookmark_ = EvtCreateBookmark(NULL);  // Create new empty bookmark for tracking
            if (bookmark_ == NULL) {
                auto error = GetLastError();
                logger->recoverable_error("EventLogSubscription::subscribe()> Failed to create empty bookmark (error %d)\n", error);
                return;
            }
            logger->debug("EventLogSubscription::subscribe()> Future-only mode: subscribing to new events only for %s\n", 
                channel_buf);
        }

        logger->debug2("EventLogSubscription::subscribe()> Attempting subscription to %s with flags %d and bookmark %p (tracking bookmark %p)\n", 
            channel_buf, flags, subscribe_bookmark, bookmark_);

        subscription_handle_ = EvtSubscribe(
            NULL,
            NULL,
            channel_.c_str(),
            query_.c_str(),
            subscribe_bookmark,  // Only pass bookmark for EvtSubscribeStartAfterBookmark
            this,
            EventLogSubscription::handleSubscriptionEvent,
            flags
        );

        if (subscription_handle_ == NULL) {
            auto status = GetLastError();
            logger->critical("EventLogSubscription::subscribe()> could not subscribe to %s (error %d)\n",
                channel_buf, status);
            return;
        }

        subscription_active_ = true;
        logger->debug2("EventLogSubscription::subscribe()> Successfully subscribed to %s\n", channel_buf);
    }

    void EventLogSubscription::cancelSubscription() {
    auto logger = LOG_THIS;
    if (subscription_active_) {
        // saveBookmark();
        if (subscription_handle_) {
            // Add error handling for EvtClose
            if (!EvtClose(subscription_handle_)) {
                DWORD errorCode = GetLastError();
                logger->recoverable_error("EventLogSubscription::cancelSubscription()> "
                    "Failed to close subscription handle (error %d)\n",
                    errorCode);
                // Continue execution even after error - we've logged the issue
            }
            subscription_handle_ = NULL;
        }
        subscription_active_ = false;
    }
}

    // Initialize static event counters
    std::atomic<uint64_t> EventLogSubscription::s_successfullyHandledEvents{0};
    std::atomic<uint64_t> EventLogSubscription::s_unsuccessfullyHandledEvents{0};

    // Static methods for event counters implementation
    uint64_t EventLogSubscription::getSuccessfullyHandledEventsCount() {
        return s_successfullyHandledEvents.load(std::memory_order_relaxed);
    }

    uint64_t EventLogSubscription::getUnsuccessfullyHandledEventsCount() {
        return s_unsuccessfullyHandledEvents.load(std::memory_order_relaxed);
    }

    DWORD WINAPI EventLogSubscription::handleSubscriptionEvent(
        EVT_SUBSCRIBE_NOTIFY_ACTION action, 
        PVOID pContext, 
        EVT_HANDLE hEvent) 
    {
        auto logger = LOG_THIS;
        
        try {
            // First, handle the SEH-prone parts in a separate C function
            DWORD sehResult = HandleSubscriptionSEH(action, pContext, hEvent);
            if (sehResult != ERROR_SUCCESS) {
                logger->critical("EventLogSubscription::handleSubscriptionEvent()> Structured exception: 0x%08X at %s:%d\n",
                    sehResult, __FILE__, __LINE__);
                return ERROR_SUCCESS;
            }
            
            // If we get here, we've handled the SEH-prone parts successfully
            EventLogSubscription* subscription = static_cast<EventLogSubscription*>(pContext);
            
            switch (action) {
            case EvtSubscribeActionError:
                if (hEvent && (DWORD_PTR)hEvent != ERROR_EVT_QUERY_RESULT_STALE) {
                    logger->recoverable_error("EventLogSubscription::handleSubscriptionEvent()> Received error event, error code: %lu\n",
                        (DWORD_PTR)hEvent);
                }
                break;

            case EvtSubscribeActionDeliver:
                if (hEvent && subscription) {
#if ONLY_FOR_DEBUGGING_CURRENTLY_DISABLED
                    SlidingWindowMetrics::instance().recordIncoming();
#endif

                    // Create EventLogEvent on the stack within this scope
                    EventLogEvent evt(hEvent);
                    try {
                        // Call the event handler and capture the result
                        Result result = subscription->event_handler_->handleEvent(subscription->subscription_name_.c_str(), evt);
                        if (result.isSuccess()) {
                            s_successfullyHandledEvents.fetch_add(1, std::memory_order_relaxed);
                        } else {
                            s_unsuccessfullyHandledEvents.fetch_add(1, std::memory_order_relaxed);
                            // Optional: Log the failure message from the result if needed
                            // logger->warning("Event handler reported failure for subscription '%S': %s", subscription->subscription_name_.c_str(), result.what());
                        }
                    } catch (const std::exception& e) {
                        logger->critical("EventLogSubscription::handleEvent exception: %s\n", e.what());
                        s_unsuccessfullyHandledEvents.fetch_add(1, std::memory_order_relaxed);
                    } catch (...) {
                        logger->critical("EventLogSubscription::handleEvent unknown exception\n");
                        s_unsuccessfullyHandledEvents.fetch_add(1, std::memory_order_relaxed);
                    }

                    // Update the subscription bookmark and save it if needed
                    if (subscription->updateBookmark(hEvent)) {
                        subscription->incrementedSaveBookmark();
                    }
                }
                break;
            }

            return ERROR_SUCCESS;
        } catch (const std::exception& e) {
            logger->critical("EventLogSubscription::handleSubscriptionEvent()> Exception: %s\n", e.what());
            return ERROR_SUCCESS;
        } catch (...) {
            logger->critical("EventLogSubscription::handleSubscriptionEvent()> Unknown exception\n");
            return ERROR_SUCCESS;
        }
    }

    bool EventLogSubscription::saveBookmark()
    {
        auto logger = LOG_THIS;
        if (!bookmark_modified_) {
            logger->debug3("EventLogSubscription::saveBookmark()> No changes to save for %ls\n", channel_.c_str());
            return true;
        }

        try {
            if (!bookmark_) {
                logger->debug3("EventLogSubscription::saveBookmark()> No bookmark to save for %ls\n", channel_.c_str());
                return false;
            }

            // Add validity check for the bookmark handle
            if (bookmark_ == INVALID_HANDLE_VALUE) {
                logger->recoverable_error("EventLogSubscription::saveBookmark()> Invalid bookmark handle for %ls\n", channel_.c_str());
                return false;
            }

            // First call EvtRender with NULL buffer to get required size
            DWORD bufferSize = 0;
            DWORD propertyCount = 0;
            if (!EvtRender(NULL, bookmark_, EvtRenderBookmark, 0, NULL, &bufferSize, &propertyCount)) {
                DWORD error = GetLastError();
                if (error != ERROR_INSUFFICIENT_BUFFER) {  // This error is expected when getting buffer size
                    logger->recoverable_error("EventLogSubscription::saveBookmark()> Failed to get required buffer size, error: %lu\n",
                        error);
                    return false;
                }
            }

            // Typical bookmark XML is small, but let's be safe
            if (bufferSize > MAX_BOOKMARK_SIZE) {
                logger->recoverable_error("EventLogSubscription::saveBookmark()> Bookmark size %lu exceeds maximum %lu\n",
                    bufferSize, MAX_BOOKMARK_SIZE);
                return false;
            }

            // Get buffer from global pool
            DWORD bufferUsed = 0;

            // Suppress false positive warning - bookmark_xml_buffer_ is a fixed array, not a pointer
            #pragma warning(push)
            #pragma warning(disable: 6387)
            // Now render the bookmark into our buffer
            if (!EvtRender(
                NULL, 
                bookmark_, 
                EvtRenderBookmark, 
                MAX_BOOKMARK_SIZE, 
                bookmark_xml_buffer_, 
                &bufferUsed, 
                &propertyCount)) {
                DWORD error = GetLastError();
                logger->recoverable_error("EventLogSubscription::saveBookmark()> Failed to render bookmark, error: %lu\n",
                    error);
                return false;
            }
            #pragma warning(pop)

            try {
                Registry::writeBookmark(channel_.c_str(), bookmark_xml_buffer_, bufferUsed * sizeof(wchar_t));
                bookmark_modified_ = false;
                events_since_last_save_ = 0;
                last_bookmark_save_ = time(nullptr);
                logger->debug2("EventLogSubscription::saveBookmark()> Saved bookmark for %ls\n", channel_.c_str());
                return true;
            }
            catch (const Result& r) {
                logger->recoverable_error("EventLogSubscription::saveBookmark()> Failed to save bookmark: %s\n", r.what());
                return false;
            }
        }
        catch (const std::exception& e) {
            logger->recoverable_error("EventLogSubscription::saveBookmark()> Exception: %s\n", e.what());
            return false;
        }
        catch (...) {
            logger->recoverable_error("EventLogSubscription::saveBookmark()> Unknown exception\n");
            return false;
        }
    }

    bool EventLogSubscription::updateBookmark(EVT_HANDLE hEvent) {
        auto logger = LOG_THIS;
        
        try {
            // Check if both handles are valid
            if (!bookmark_ || !hEvent) {
                logger->debug3("EventLogSubscription::updateBookmark()> Invalid handles: bookmark=%p, event=%p\n",
                    bookmark_, hEvent);
                return false;
            }
            
            // Additional validity check
            if (bookmark_ == INVALID_HANDLE_VALUE || hEvent == INVALID_HANDLE_VALUE) {
                logger->recoverable_error("EventLogSubscription::updateBookmark()> Invalid handle values\n");
                return false;
            }

            BOOL updateResult = EvtUpdateBookmark(bookmark_, hEvent);
            if (!updateResult) {
                DWORD errorCode = GetLastError();
                logger->debug2("EventLogSubscription::updateBookmark()> Failed to update bookmark, error: %lu\n", errorCode);
                return false;
            }

            bookmark_modified_ = true;
            return true;
        } catch (const std::exception& e) {
            logger->recoverable_error("EventLogSubscription::updateBookmark()> Exception: %s\n", e.what());
            return false;
        } catch (...) {
            logger->recoverable_error("EventLogSubscription::updateBookmark()> Unknown exception\n");
            return false;
        }
    }

} // namespace Syslog_agent