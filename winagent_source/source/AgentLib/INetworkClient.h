#pragma once

#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Configuration.h"
#include "../Infrastructure/Logger.h"
#include <cstring>

namespace Syslog_agent {

    // Define shared network error codes for all client implementations
    enum class NetworkErrorCode {
        // INetworkClient base error codes (-1 to 99)
        Success = 0,
        Unknown = 1,
        NotConnected = 2,
        ConnectionFailed = 3,
        InvalidParameters = 4,
        SendFailed = 5,
        ReceiveFailed = 6,
        Timeout = 7,
        MemoryError = 8,
        AuthenticationFailed = 9,
        
        // JsonNetworkClient specific error codes (100 to 199)
        JsonInvalidSocket = 100,
        JsonSendError = 101,
        JsonIncompleteSend = 102,
        JsonReceiveError = 103,
        JsonMalformedResponse = 104,
        
        // HttpNetworkClient specific error codes (200 to 249)
        HttpRequestCreationFailed = 200,
        HttpHeadersError = 201,
        HttpRequestSendFailed = 202,
        HttpResponseError = 203,
        HttpQueryError = 204,
        HttpTlsError = 205,
        
        // HTTP status codes (250 to 299)
        // Map HTTP status codes to our error range: 250 + (status_code % 1000)
        // This ensures all HTTP status codes fit in our range while maintaining uniqueness
        Http200OK = 250,          // 250 + (200 % 1000) = 250 + 200 = 450, but we'll use 250 for OK
        Http400BadRequest = 250 + (400 % 50),      // 250 + 0 = 250
        Http401Unauthorized = 250 + (401 % 50),    // 250 + 1 = 251
        Http403Forbidden = 250 + (403 % 50),       // 250 + 3 = 253
        Http404NotFound = 250 + (404 % 50),        // 250 + 4 = 254
        Http405MethodNotAllowed = 250 + (405 % 50), // 250 + 5 = 255
        Http408RequestTimeout = 250 + (408 % 50),   // 250 + 8 = 258
        Http429TooManyRequests = 250 + (429 % 50),  // 250 + 29 = 279
        Http500ServerError = 290,      // Avoid conflict with Http400BadRequest
        Http502BadGateway = 292,       // Avoid conflict
        Http503ServiceUnavailable = 293, // Avoid conflict with Http403Forbidden
        Http504GatewayTimeout = 294     // Avoid conflict with Http404NotFound
    };
    
    // Get human-readable descriptive message for HTTP status code
    inline const char* HttpStatusToString(int http_status) {
        switch (http_status) {
            // 2xx - Success
            case 200: return "OK";
            case 201: return "Created";
            case 202: return "Accepted";
            case 204: return "No Content";
            
            // 3xx - Redirection
            case 301: return "Moved Permanently";
            case 302: return "Found";
            case 303: return "See Other";
            case 304: return "Not Modified";
            case 307: return "Temporary Redirect";
            case 308: return "Permanent Redirect";
            
            // 4xx - Client Errors
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 406: return "Not Acceptable";
            case 408: return "Request Timeout";
            case 409: return "Conflict";
            case 410: return "Gone";
            case 413: return "Payload Too Large";
            case 414: return "URI Too Long";
            case 415: return "Unsupported Media Type";
            case 429: return "Too Many Requests";
            
            // 5xx - Server Errors
            case 500: return "Internal Server Error";
            case 501: return "Not Implemented";
            case 502: return "Bad Gateway";
            case 503: return "Service Unavailable";
            case 504: return "Gateway Timeout";
            case 505: return "HTTP Version Not Supported";
            
            default:
                if (http_status >= 200 && http_status < 300) return "Success";
                if (http_status >= 300 && http_status < 400) return "Redirection";
                if (http_status >= 400 && http_status < 500) return "Client Error";
                if (http_status >= 500 && http_status < 600) return "Server Error";
                return "Unknown HTTP Status";
        }
    }
    
    // Define log severity constants to avoid namespace issues
    enum HttpLogSeverity {
        LOG_DEBUG = 0,           // Success cases (200-299)
        LOG_RECOVERABLE = 1,     // Recoverable errors (most 4xx)
        LOG_CRITICAL = 2,        // Temporary server errors (5xx, 429)
        LOG_FATAL = 3            // Fatal errors requiring user intervention (401, 403, 404, 405)
    };
    
    // Determine appropriate log severity for HTTP status codes
    inline HttpLogSeverity HttpStatusToLogSeverity(int http_status) {
        if (http_status >= 200 && http_status < 300) {
            return LOG_DEBUG; // Success
        }
        
        // Determine severity based on HTTP status code ranges
        if (http_status >= 500) {
            // 5xx errors are usually temporary server issues
            return LOG_CRITICAL;
        }
        
        // Specific 4xx codes that likely require user intervention
        switch (http_status) {
            case 401: // Unauthorized - needs API key/auth
            case 403: // Forbidden - permissions issue
                return LOG_FATAL;
                
            case 404: // Not Found - resource doesn't exist
            case 405: // Method Not Allowed - API endpoint issue
                return LOG_FATAL;
                
            case 429: // Too Many Requests - rate limiting
                return LOG_CRITICAL;
                
            default: 
                // Other 4xx errors might be recoverable
                return LOG_RECOVERABLE;
        }
    }
    
    // Convert HTTP status code to our error code range
    inline NetworkErrorCode HttpStatusToErrorCode(int http_status) {
        if (http_status == 200) return NetworkErrorCode::Http200OK;
        
        // Handle specific HTTP errors we've defined explicitly
        switch (http_status) {
            case 400: return NetworkErrorCode::Http400BadRequest;
            case 401: return NetworkErrorCode::Http401Unauthorized;
            case 403: return NetworkErrorCode::Http403Forbidden;
            case 404: return NetworkErrorCode::Http404NotFound;
            case 405: return NetworkErrorCode::Http405MethodNotAllowed;
            case 408: return NetworkErrorCode::Http408RequestTimeout;
            case 429: return NetworkErrorCode::Http429TooManyRequests;
            case 500: return NetworkErrorCode::Http500ServerError;
            case 502: return NetworkErrorCode::Http502BadGateway;
            case 503: return NetworkErrorCode::Http503ServiceUnavailable;
            case 504: return NetworkErrorCode::Http504GatewayTimeout;
        }
        
        // For other HTTP status codes, generate a unique ID in our range
        // Use the last 2 digits of status code when possible
        return static_cast<NetworkErrorCode>(250 + (http_status % 50));
    }
    
    // Define specific error codes for initialization
    enum class InitializeError {
        Success = 0,
        InvalidHost,
        InvalidApiKey,
        InvalidUrl,
        UrlTooLong,
        ApiKeyTooLong,
        WinHttpCrackUrlFailed,
        WinHttpOpenFailed,      // Failed WinHttpOpen
        WinHttpSetOptionFailed, // Failed WinHttpSetOption (timeouts, security, etc.)
        TlsConfigError,         // General TLS/SSL configuration issue (placeholder, more specific errors can be added)
        AllocationFailed,       // Memory allocation failure (placeholder)
        UnknownError
    };

    // Helper to convert InitializeError to a string for logging
    inline const char* InitializeErrorToString(InitializeError err) {
        switch (err) {
            case InitializeError::Success:                  return "Success";
            case InitializeError::InvalidHost:              return "Invalid or empty host name parsed from URL";
            case InitializeError::InvalidApiKey:            return "API key is null or too long";
            case InitializeError::InvalidUrl:               return "URL is null, empty, or too long";
            case InitializeError::UrlTooLong:               return "Provided URL exceeds maximum length";
            case InitializeError::ApiKeyTooLong:            return "Provided API key exceeds maximum length";
            case InitializeError::WinHttpCrackUrlFailed:    return "WinHttpCrackUrl failed to parse the URL";
            case InitializeError::WinHttpOpenFailed:        return "WinHttpOpen failed to create a session";
            case InitializeError::WinHttpSetOptionFailed:   return "WinHttpSetOption failed (e.g., timeouts, TLS settings)";
            case InitializeError::TlsConfigError:           return "TLS/SSL configuration error";
            case InitializeError::AllocationFailed:         return "Memory allocation failed";
            case InitializeError::UnknownError:             return "An unknown error occurred during initialization";
            default:                                        return "Unknown InitializeError code";
        }
    }

    class NetworkResult {
    public:
        static constexpr size_t MAX_MESSAGE_LENGTH = 1024;

        // Constructor for success
        NetworkResult() : error_number_(static_cast<int>(NetworkErrorCode::Success)) {
            message_[0] = '\0';
            code_[0] = '\0';
        }

        // Constructor for error with code and optional message
        NetworkResult(NetworkErrorCode error_code, const char* message = nullptr) 
            : error_number_(static_cast<int>(error_code)) {
            if (message) {
                strncpy_s(message_, message, MAX_MESSAGE_LENGTH - 1);
            }
            else {
                message_[0] = '\0';
            }
            code_[0] = '\0';
        }

        // Constructor for backward compatibility with DWORD error codes
        NetworkResult(DWORD win32_error, const char* message = nullptr) 
            : error_number_(win32_error) {
            if (message) {
                strncpy_s(message_, message, MAX_MESSAGE_LENGTH - 1);
            }
            else {
                message_[0] = '\0';
            }
            code_[0] = '\0';
        }

        // Implicit conversion to int for easy numeric comparisons
        operator int() const { return error_number_; }

        // Equality operators for NetworkErrorCode
        bool operator==(NetworkErrorCode other) const { return error_number_ == static_cast<int>(other); }
        bool operator!=(NetworkErrorCode other) const { return error_number_ != static_cast<int>(other); }
        
        // Equality operators for backward compatibility
        bool operator==(int other) const { return error_number_ == other; }
        bool operator!=(int other) const { return error_number_ != other; }

        // Getters
        int getErrorNumber() const { return error_number_; }
        const char* getCode() const { return code_; }
        const char* getMessage() const { return message_; }
        bool hasMessage() const { return message_[0] != '\0'; }

    private:
        int error_number_; // Now stores NetworkErrorCode as int
        char code_[16];   // String code (e.g., "HTTP404", "SENDERR")
        char message_[MAX_MESSAGE_LENGTH]; // Detailed error message
    };

    class INetworkClient {
    public:
        using RESULT_TYPE = NetworkResult;
        static const RESULT_TYPE RESULT_SUCCESS; // Defined in cpp file

        virtual ~INetworkClient() = default;

        virtual InitializeError initialize(bool is_primary, const Configuration* config, const wchar_t* api_key,
            const wchar_t* url, unsigned int port = 0) = 0;
        virtual bool connect() = 0;
        virtual RESULT_TYPE post(const char* buf, uint32_t length) = 0;
        virtual void close() = 0;
        virtual bool getLogzillaVersion(char* version_buf, size_t max_length, size_t& bytes_written) = 0;
        virtual SOCKET getSocket() = 0;

    protected:
        INetworkClient() = default;
    };

} // namespace Syslog_agent
#pragma once
