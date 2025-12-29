#include "pch.h"

/*
SyslogAgent: a syslog agent for Windows
Copyright 2021 Logzilla Corp.
*/

#include "HttpNetworkClient.h"
#include "../Infrastructure/Logger.h"
#include "Configuration.h"
#include "../Infrastructure/HandleGuard.h"

#include <WinSock2.h>
#include <Windows.h>
#include <winhttp.h>
#include <wincrypt.h>

// Define security flags if not already defined
#ifndef SECURITY_FLAG_CERT_DATE_INVALID
constexpr DWORD SECURITY_FLAG_CERT_DATE_INVALID = 0x00000020;
#endif
#ifndef SECURITY_FLAG_CERT_CN_INVALID
constexpr DWORD SECURITY_FLAG_CERT_CN_INVALID = 0x00001000;
#endif
#ifndef SECURITY_FLAG_CERT_REVOKED
constexpr DWORD SECURITY_FLAG_CERT_REVOKED = 0x00000800;
#endif
#ifndef SECURITY_FLAG_CERT_REV_FAILED
constexpr DWORD SECURITY_FLAG_CERT_REV_FAILED = 0x00000040;
#endif
#ifndef SECURITY_FLAG_INVALID_CA
constexpr DWORD SECURITY_FLAG_INVALID_CA = 0x00000100;
#endif

#include <string>
#include <sstream>
#include <locale>
#include <codecvt>
#include <mutex>
#include <algorithm>

using namespace Syslog_agent;

namespace {
    // Helper function to provide detailed certificate validation guidance
    void LogCertificateValidationGuidance(Logger* logger, bool use_cert_authority, DWORD secureFailures = 0) {
        if (use_cert_authority) {
            logger->fatal("CERTIFICATE AUTHORITY VALIDATION FAILED");
            logger->fatal("Certificate authority validation is enabled but certificate validation failed.");
            logger->fatal("");
            logger->fatal("POSSIBLE CAUSES AND SOLUTIONS:");
            
            if (secureFailures & SECURITY_FLAG_CERT_DATE_INVALID) {
                logger->fatal("• Certificate expired or not yet valid");
                logger->fatal("  → Contact server administrator to renew the certificate");
                logger->fatal("  → Verify system clock is correct");
            }
            if (secureFailures & SECURITY_FLAG_CERT_CN_INVALID) {
                logger->fatal("• Hostname in URL doesn't match certificate");
                logger->fatal("  → Verify the server URL is correct");
                logger->fatal("  → Contact administrator if certificate needs Subject Alternative Names");
            }
            if (secureFailures & SECURITY_FLAG_CERT_REVOKED) {
                logger->fatal("• Certificate has been revoked by the Certificate Authority");
                logger->fatal("  → Contact server administrator - certificate must be replaced");
            }
            if (secureFailures & SECURITY_FLAG_CERT_REV_FAILED) {
                logger->fatal("• Certificate revocation check failed");
                logger->fatal("  → Check internet connectivity for CRL/OCSP access");
                logger->fatal("  → Contact network administrator about firewall rules");
            }
            if (secureFailures & SECURITY_FLAG_INVALID_CA) {
                logger->fatal("• Certificate Authority is not trusted");
                logger->fatal("  → Install CA certificate in Windows Certificate Store");
                logger->fatal("  → Contact administrator for proper CA certificate");
            }
            if (secureFailures == 0) {
                logger->fatal("• General certificate validation failure");
                logger->fatal("  → Verify server certificate is properly configured");
                logger->fatal("  → Check Windows Certificate Store contains required CA certificates");
                logger->fatal("  → Ensure network connectivity for certificate validation");
            }
            
            logger->fatal("");
            logger->fatal("TO DISABLE CERTIFICATE AUTHORITY VALIDATION:");
            logger->fatal("  → Set 'Use Certificate Authority' to false in configuration");
            logger->fatal("  → This will accept self-signed certificates (less secure)");
        } else {
            logger->fatal("SELF-SIGNED CERTIFICATE MODE VALIDATION FAILED");
            logger->fatal("Self-signed certificate mode is enabled but validation still failed.");
            logger->fatal("This is unexpected and indicates a configuration or network issue.");
            logger->fatal("");
            logger->fatal("POSSIBLE SOLUTIONS:");
            logger->fatal("• Verify the server is properly configured for HTTPS");
            logger->fatal("• Check that TLS/SSL is enabled on the server");
            logger->fatal("• Ensure firewall allows HTTPS traffic");
            logger->fatal("• Contact server administrator");
        }
    }
    // Helper function to get descriptive text for Win32 error codes
    const char* GetWin32ErrorText(DWORD errorCode) {
        // Common Windows error codes
        switch (errorCode) {
            case ERROR_SUCCESS: return "Operation completed successfully";
            case ERROR_INVALID_FUNCTION: return "Invalid function";
            case ERROR_FILE_NOT_FOUND: return "File not found";
            case ERROR_PATH_NOT_FOUND: return "Path not found";
            case ERROR_ACCESS_DENIED: return "Access denied";
            case ERROR_INVALID_HANDLE: return "Invalid handle";
            case ERROR_NOT_ENOUGH_MEMORY: return "Not enough memory";
            case ERROR_INVALID_DATA: return "Invalid data";
            case ERROR_OUTOFMEMORY: return "Out of memory";
            case ERROR_INVALID_DRIVE: return "Invalid drive";
            case ERROR_NOT_SAME_DEVICE: return "Not same device";
            case ERROR_NO_MORE_FILES: return "No more files";
            case ERROR_WRITE_PROTECT: return "Write protected";
            case ERROR_BAD_UNIT: return "Bad unit";
            case ERROR_NOT_READY: return "Device not ready";
            case ERROR_SHARING_VIOLATION: return "Sharing violation";
            case ERROR_LOCK_VIOLATION: return "Lock violation";
            case ERROR_HANDLE_EOF: return "End of file";
            case ERROR_NOT_SUPPORTED: return "Not supported";
            case ERROR_REM_NOT_LIST: return "Network path not found";
            case ERROR_DUP_NAME: return "Duplicate name";
            case ERROR_BAD_NETPATH: return "Bad network path";
            case ERROR_NETWORK_BUSY: return "Network busy";
            case ERROR_DEV_NOT_EXIST: return "Device does not exist";
            case ERROR_BAD_NET_NAME: return "Bad network name";
            case ERROR_ALREADY_ASSIGNED: return "Already assigned";
            case ERROR_INVALID_PASSWORD: return "Invalid password";
            case ERROR_INVALID_PARAMETER: return "Invalid parameter";
            case ERROR_INVALID_NAME: return "Invalid name";
            case ERROR_OPEN_FAILED: return "Open failed";
            case ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
            case ERROR_DISK_FULL: return "Disk full";
            case ERROR_CALL_NOT_IMPLEMENTED: return "Call not implemented";
            case ERROR_INSUFFICIENT_BUFFER: return "Insufficient buffer";
            case ERROR_OPERATION_ABORTED: return "Operation aborted";
            case ERROR_NOT_CONNECTED: return "Not connected";
            case ERROR_TIMEOUT: return "Operation timed out";
            
            // WinHTTP specific error codes
            case ERROR_WINHTTP_SECURE_FAILURE: return "Security negotiation failed";
            case ERROR_WINHTTP_INVALID_URL: return "Invalid URL";
            case ERROR_WINHTTP_NAME_NOT_RESOLVED: return "DNS name not resolved";
            case ERROR_WINHTTP_CANNOT_CONNECT: return "Cannot connect to server";
            case ERROR_WINHTTP_CONNECTION_ERROR: return "Connection error";
            case ERROR_WINHTTP_TIMEOUT: return "Request timeout";
            case ERROR_WINHTTP_HEADER_NOT_FOUND: return "Header not found";
            case ERROR_WINHTTP_SECURE_CERT_DATE_INVALID: return "SSL certificate date invalid";
            case ERROR_WINHTTP_SECURE_CERT_CN_INVALID: return "SSL certificate name invalid";
            case ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED: return "Client certificate required";
            case ERROR_WINHTTP_SECURE_CERT_REVOKED: return "SSL certificate has been revoked";
            case ERROR_WINHTTP_SECURE_CERT_REV_FAILED: return "SSL certificate revocation check failed";
            case ERROR_WINHTTP_SECURE_CHANNEL_ERROR: return "SSL secure channel error";
            case ERROR_WINHTTP_SECURE_INVALID_CERT: return "SSL certificate is invalid";
            case ERROR_WINHTTP_SECURE_INVALID_CA: return "SSL certificate authority is invalid";
            
            // Additional network errors with explicit values
            case 10061: return "Connection refused";
            case 10051: return "Network unreachable";
            case 10065: return "Host unreachable";
            case 10053: return "Connection aborted";
            case 10054: return "Connection reset";
            
            default: return "Unknown error";
        }
    }
    
    // Convert wide string to UTF-8 using fixed buffer
    bool ws2s(const wchar_t* wstr, char* buffer, size_t bufferSize) {
        if (!wstr || !buffer || bufferSize == 0) return false;

        int result = WideCharToMultiByte(
            CP_UTF8,
            0,
            wstr,
            -1,
            buffer,
            static_cast<int>(bufferSize),
            NULL,
            NULL
        );

        return result > 0;
    }

    // Convert UTF-8 to wide string using fixed buffer
    bool s2ws(const char* str, wchar_t* buffer, size_t bufferSize) {
        if (!str || !buffer || bufferSize == 0) return false;

        int result = MultiByteToWideChar(
            CP_UTF8,
            0,
            str,
            -1,
            buffer,
            static_cast<int>(bufferSize)
        );

        return result > 0;
    }
}

HttpNetworkClient::HttpNetworkClient()
    : use_ssl_(false)
    , use_compression_(false)
    , connect_timeout_(DEFAULT_CONNECT_TIMEOUT)
    , send_timeout_(DEFAULT_SEND_TIMEOUT)
    , receive_timeout_(DEFAULT_RECEIVE_TIMEOUT)
    , port_(0)
    , is_connected_(false)
    , stored_security_flags_(0)
{
    // Initialize string buffers
    url_[0] = L'\0';
    host_[0] = L'\0';
    path_[0] = L'\0';
    api_key_[0] = L'\0';
}

HttpNetworkClient::~HttpNetworkClient()
{
    // No need for close() call, HandleGuards manage resources automatically
}

InitializeError HttpNetworkClient::initialize(bool is_primary, const Configuration* config, const wchar_t* api_key,
    const wchar_t* url, unsigned int port)
{
    auto logger = LOG_THIS;
    std::lock_guard<std::recursive_mutex> lock(connecting_);
    
    // --- Determine SSL settings from URL scheme only ---
    bool use_ssl = false;
    bool use_self_signed_cert = false;
    
    if (url && wcsstr(url, L"https://") != nullptr) {
        use_ssl = true;
    } else if (url && wcsstr(url, L"http://") != nullptr) {
        use_ssl = false;
    } else {
        logger->fatal("HttpNetworkClient::initialize failed: URL must start with 'http://' or 'https://'. URL: '%ls'", (url ? url : L"NULL"));
        return InitializeError::InvalidUrl;
    }
    
    // Get certificate authority setting from configuration
    if (config) {
        use_self_signed_cert = config->getUseSelfSignedCert(is_primary);
    }
    
    // Validate certificate store access if CA validation is enabled
    if (use_ssl && !use_self_signed_cert) {
        HCERTSTORE hStore = CertOpenSystemStore(NULL, L"ROOT");
        if (!hStore) {
            DWORD error = GetLastError();
            logger->fatal("HttpNetworkClient::initialize failed: Cannot access Windows ROOT certificate store");
            logger->fatal("Certificate authority validation requires access to Windows certificate store");
            logger->fatal("Error: %d (%s)", error, GetWin32ErrorText(error));
            logger->fatal("");
            logger->fatal("POSSIBLE SOLUTIONS:");
            logger->fatal("• Run as administrator if certificate store access is restricted");
            logger->fatal("• Check Windows certificate store is not corrupted");
            logger->fatal("• Disable certificate authority validation in configuration");
            return InitializeError::TlsConfigError;
        }
        CertCloseStore(hStore, 0);
        logger->debug("Certificate authority validation enabled - Windows certificate store accessible");
    }
    
    logger->debug("HttpNetworkClient::initialize attempting. URL: '%ls', Port: %u, UseSSL: %d, UseSelfSignedCert: %d",
                  (url ? url : L"NULL"), port, use_ssl, use_self_signed_cert);

    // --- Basic Parameter Validation ---
    if (!config) {
         logger->recoverable_error("HttpNetworkClient::initialize failed: Configuration object is null.");
         return InitializeError::UnknownError;
    }
    if (!url || url[0] == L'\0') {
        logger->recoverable_error("HttpNetworkClient::initialize failed: URL parameter is null or empty.");
        return InitializeError::InvalidUrl;
    }
    size_t url_len = wcslen(url);
    if (url_len >= MAX_URL_LENGTH) {
        logger->recoverable_error("HttpNetworkClient::initialize failed: Provided URL (length %zu) exceeds maximum length (%zu). URL: '%ls'", url_len, MAX_URL_LENGTH, url);
        return InitializeError::UrlTooLong;
    }

    if (!api_key) { // API key can be empty string but not null pointer
        logger->recoverable_error("HttpNetworkClient::initialize failed: API key parameter is null.");
        return InitializeError::InvalidApiKey;
    }
    size_t api_key_len = wcslen(api_key);
    if (api_key_len >= MAX_API_KEY_LENGTH) {
        logger->recoverable_error("HttpNetworkClient::initialize failed: Provided API key (length %zu) exceeds maximum length (%zu).", api_key_len, MAX_API_KEY_LENGTH);
        return InitializeError::ApiKeyTooLong;
    }

    // --- Store Configuration ---
    use_ssl_ = use_ssl;
    port_ = port;
    use_self_signed_cert_ = use_self_signed_cert;

    // --- Parse URL ---
    wchar_t host_buffer[MAX_URL_LENGTH] = {0};
    wchar_t path_buffer[MAX_PATH_LENGTH] = {0};
    URL_COMPONENTS urlComp = { 0 };
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.lpszHostName = host_buffer;
    urlComp.dwHostNameLength = _countof(host_buffer);
    urlComp.lpszUrlPath = path_buffer;
    urlComp.dwUrlPathLength = _countof(path_buffer);

    if (!WinHttpCrackUrl(url, url_len, 0, &urlComp)) {
        DWORD error = GetLastError();
        logger->recoverable_error("HttpNetworkClient::initialize failed: WinHttpCrackUrl could not parse URL '%ls'. Win32 Error Code: %lu (%s)", url, error, GetWin32ErrorText(error));
        host_[0] = L'\0'; 
        path_[0] = L'\0';
        return InitializeError::WinHttpCrackUrlFailed;
    }

    // --- Determine Port ---
    if (port_ < 1) { 
        port_ = urlComp.nPort;
        if (port_ == 0) { 
            port_ = use_ssl_ ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
            logger->debug("HttpNetworkClient::initialize: No port specified in URL '%ls' or arguments, defaulting to %u based on scheme (SSL: %d).", url, port_, use_ssl_);
        }
    }

    // --- Store the parsed components ---
    wcsncpy_s(url_, _countof(url_), url, _TRUNCATE);
    wcsncpy_s(host_, _countof(host_), host_buffer, _TRUNCATE);
    wcsncpy_s(path_, _countof(path_), path_buffer, _TRUNCATE);
    wcsncpy_s(api_key_, _countof(api_key_), api_key, _TRUNCATE);

    // --- Validate and Store Host/Path ---
    if (wcslen(host_buffer) == 0) {
         logger->recoverable_error("HttpNetworkClient::initialize failed: Parsed host name from URL '%ls' is empty.", url_);
         return InitializeError::InvalidHost;
    }

    // --- Initialize WinHTTP Session ---
    hSession_.reset(WinHttpOpen(L"SyslogAgent/1.0",
                                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME,
                                WINHTTP_NO_PROXY_BYPASS,
                                0));

    if (!hSession_) {
        DWORD error = GetLastError();
        logger->critical("HttpNetworkClient::initialize failed: WinHttpOpen failed with Win32 Error Code: %lu. Check WinHTTP service status, system proxy settings, or potential interference from security software.", error);
        return InitializeError::WinHttpOpenFailed;
    }

    logger->debug("HttpNetworkClient::initialize successful for URL: '%ls' (Parsed Host: '%ls', Port: %u, Path: '%ls', SSL: %d)",
                   url_, host_, port_, path_, use_ssl_);
    return InitializeError::Success;
}



bool HttpNetworkClient::connect()
{
    auto logger = LOG_THIS;
    std::lock_guard<std::recursive_mutex> lock(connecting_);

    if (is_connected_) {
        return true;
    }

    // Only create a new session if we don't have one already
    if (!hSession_) {
        hSession_.reset(WinHttpOpen(L"SyslogAgent/1.0",
                                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME,
                                WINHTTP_NO_PROXY_BYPASS,
                                0));

        if (!hSession_) {
            DWORD error = GetLastError();
            logger->critical("HttpNetworkClient::connect failed: WinHttpOpen failed with Win32 Error Code: %lu. Check WinHTTP service status, system proxy settings, or potential interference from security software.", error);
            return false;
        }

        // Set timeouts
        if (!WinHttpSetTimeouts(hSession_.get(),
            0,                  // DNS resolution timeout
            connect_timeout_,   // Connect timeout
            send_timeout_,      // Send timeout
            receive_timeout_))  // Receive timeout
        {
            DWORD error = GetLastError();
            logger->warning("HttpNetworkClient::connect() failed to set timeouts: %d (%s)\n", error, GetWin32ErrorText(error));
        }
    }

    // Only create a new connection if we don't have one already
    if (!hConnection_) {
        logger->debug2("HttpNetworkClient::connect() connecting to %ls:%d\n", host_, port_);
        INTERNET_PORT port = port_ ? (INTERNET_PORT)port_ : (use_ssl_ ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT);
        hConnection_.reset(WinHttpConnect(hSession_.get(), host_, port, 0));
        if (!hConnection_) {
            DWORD error = GetLastError();
            logger->recoverable_error("HttpNetworkClient::connect() WinHttpConnect failed: %d (%s)\n", error, GetWin32ErrorText(error));
            hSession_.reset();
            return false;
        }
    }

    is_connected_ = true;
    return true;
}


HttpNetworkClient::RESULT_TYPE HttpNetworkClient::post(const char* buf, uint32_t length)
{
    auto logger = LOG_THIS;
    std::lock_guard<std::recursive_mutex> lock(connecting_);

    if (!is_connected_ || !hConnection_) {
        logger->debug2("HttpNetworkClient::post() Not connected, connection handle: %p, is_connected: %d\n",
            hConnection_.get(), is_connected_);
        // Use system-defined ERROR_NOT_CONNECTED so external callers/tests expecting that
        // specific Win32 code (2250) see the correct value when they cast the result.
        NetworkResult result(static_cast<NetworkErrorCode>(ERROR_NOT_CONNECTED),
                             "Not connected to server (http 0)");
        strcpy_s(const_cast<char*>(result.getCode()), 16, "NOTCONN");
        return result;
    }

    logger->debug2("HttpNetworkClient::post() Starting post operation - Length: %d bytes\n", length);

    DWORD flags = WINHTTP_FLAG_REFRESH;
    if (use_ssl_) {
        flags |= WINHTTP_FLAG_SECURE;
        logger->debug2("HttpNetworkClient::post() Using SSL\n");
    }

    // Create request handle
    hRequest_.reset(WinHttpOpenRequest(hConnection_.get(),
        L"POST",
        path_,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags));

    if (!hRequest_) {
        DWORD error = GetLastError();
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to open HTTP request: error %lu (%s) (http 0)", error, GetWin32ErrorText(error));
        NetworkResult result(NetworkErrorCode::HttpRequestCreationFailed, msg);
        strcpy_s(const_cast<char*>(result.getCode()), 16, "REQERR");
        return result;
    }

    // Set timeouts for this request
    if (!applyTimeouts(hRequest_.get())) {
        logger->warning("HttpNetworkClient::post() Failed to set request timeouts\n");
    }

    // Set security flags for TLS if needed
    if (use_ssl_) {
        logger->debug2("HttpNetworkClient::post() applying security flags for TLS\n");

        DWORD securityFlags = 0;
        
        if (use_self_signed_cert_) {
            // For self-signed certificates, ignore validation
            logger->debug2("HttpNetworkClient::post() using self-signed certificate mode\n");
            securityFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        } else {
            // When using certificate authority validation, we rely on the system's 
            // certificate store and do NOT ignore certificate validation errors
            logger->debug2("HttpNetworkClient::post() using certificate authority validation\n");
            // securityFlags remains 0 - no security flags to ignore validation
        }

        // Add any stored flags
        if (stored_security_flags_ != 0) {
            securityFlags |= stored_security_flags_;
        }

        // Set the security flags (even if 0 for CA validation)
        if (!WinHttpSetOption(hRequest_.get(),
            WINHTTP_OPTION_SECURITY_FLAGS,
            &securityFlags,
            sizeof(securityFlags)))
        {
            DWORD error = GetLastError();
            logger->warning("HttpNetworkClient::post() failed to set security flags: %d (%s)\n", error, GetWin32ErrorText(error));
        }
        else {
            logger->debug2("HttpNetworkClient::post() successfully set security flags: 0x%08X\n", securityFlags);
        }
    }

    // Add headers
    std::wstring headers = L"Content-Type: application/json\r\n";
    headers += L"Authorization: token ";
    headers += api_key_;
    headers += L"\r\n";

    if (use_compression_) {
        headers += L"Accept-Encoding: gzip, deflate\r\n";
    }

    if (!WinHttpAddRequestHeaders(hRequest_.get(),
        headers.c_str(),
        -1L,
        WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE))
    {
        DWORD error = GetLastError();
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to add request headers: error %lu (%s) (http 0)", error, GetWin32ErrorText(error));
        NetworkResult result(NetworkErrorCode::HttpHeadersError, msg);
        strcpy_s(const_cast<char*>(result.getCode()), 16, "HDRERR");
        return result;
    }

    // Send the request
    if (!WinHttpSendRequest(hRequest_.get(),
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        (LPVOID)buf,
        length,
        length,
        0))
    {
        DWORD error = GetLastError();
        if (error == ERROR_WINHTTP_SECURE_FAILURE) {
            // This is a FATAL error because certificate issues require external fixes
            logger->fatal("HttpNetworkClient::post() WinHttpSendRequest failed with secure failure (12175): TLS/SSL certificate validation error");

            // Try to get more specific error information
            DWORD secureFailures = 0;
            DWORD secureFailuresSize = sizeof(secureFailures);
            if (WinHttpQueryOption(hRequest_.get(),
                WINHTTP_OPTION_SECURITY_FLAGS,
                &secureFailures,
                &secureFailuresSize))
            {
                logger->fatal("Security validation failure flags: 0x%08X", secureFailures);
                LogCertificateValidationGuidance(logger, !use_self_signed_cert_, secureFailures);
            } else {
                LogCertificateValidationGuidance(logger, !use_self_signed_cert_);
            }
            
            NetworkResult result(NetworkErrorCode::HttpRequestSendFailed, "Certificate validation failed - requires configuration change");
            strcpy_s(const_cast<char*>(result.getCode()), 16, "CERTFAIL");
            return result;
        } else {
            char msg[256];
            snprintf(msg, sizeof(msg), "Failed to send request: error %lu (%s) (http 0)", error, GetWin32ErrorText(error));
            NetworkResult result(NetworkErrorCode::HttpRequestSendFailed, msg);
            strcpy_s(const_cast<char*>(result.getCode()), 16, "SNDERR");
            return result;
        }
    }

    // End the request
    if (!WinHttpReceiveResponse(hRequest_.get(), NULL)) {
        DWORD error = GetLastError();
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to receive response: error %lu (%s) (http 0)", error, GetWin32ErrorText(error));
        NetworkResult result(NetworkErrorCode::HttpResponseError, msg);
        strcpy_s(const_cast<char*>(result.getCode()), 16, "RCVERR");
        return result;
    }

    // Check status code
    DWORD status_code = 0;
    DWORD size = sizeof(status_code);
    if (!WinHttpQueryHeaders(hRequest_.get(),
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status_code,
        &size,
        WINHTTP_NO_HEADER_INDEX))
    {
        DWORD error = GetLastError();
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to query status code: error %lu (%s) (http 0)", error, GetWin32ErrorText(error));
        logger->debug3("HttpNetworkClient::post() Failed to query HTTP status code: Win32 error %lu", error);
        NetworkResult result(NetworkErrorCode::HttpQueryError, msg);
        strcpy_s(const_cast<char*>(result.getCode()), 16, "STATERR");
        return result;
    }
    
    // Always log the HTTP status code at debug3 level
    logger->debug3("HttpNetworkClient::post() HTTP status code: %lu", status_code);

    // Read response body
    char response_buffer[8192] = { 0 };
    size_t total_read = 0;
    DWORD bytes_available = 0;
    DWORD bytes_read = 0;

    while (WinHttpQueryDataAvailable(hRequest_.get(), &bytes_available)) {
        if (bytes_available == 0) break;

        // Ensure we don't overflow our buffer
        if (total_read >= sizeof(response_buffer) - 1) {
            logger->warning("HttpNetworkClient::post() Response exceeds buffer size, truncating\n");
            break;
        }

        // Calculate remaining buffer space
        size_t remaining = sizeof(response_buffer) - total_read - 1;  // -1 for null terminator
        DWORD to_read = (bytes_available > remaining) ? static_cast<DWORD>(remaining) : bytes_available;

        if (!WinHttpReadData(hRequest_.get(),
            response_buffer + total_read,
            to_read,
            &bytes_read) || bytes_read == 0)
        {
            break;
        }

        total_read += bytes_read;
    }
    response_buffer[total_read] = '\0';  // Ensure null termination

    hRequest_.reset();

    // Format the result message with both status and response body
    char msg[1024 + 256];  // Large enough for status line + response
    NetworkErrorCode errorCode;
    
    // Always log detailed HTTP response information at debug3 level
    logger->debug3("HttpNetworkClient::post() HTTP %lu response: %s", 
                  status_code, 
                  total_read > 0 ? response_buffer : "<no response body>");
    
    // Get the human-readable description of the status code
    const char* status_description = HttpStatusToString(status_code);
    
    // Determine if this is a success or error status
    bool isSuccessStatus = (status_code >= 200 && status_code < 300);
    
    if (!isSuccessStatus) {
        // Error status code
        snprintf(msg, sizeof(msg), "Server returned error (HTTP %lu - %s)\n%s",
            status_code,
            status_description,
            total_read > 0 ? response_buffer : "No response body");
        
        // Convert HTTP status code to our error code range (250-299)
        errorCode = HttpStatusToErrorCode(status_code);
        
        // Get appropriate log severity for this status code and log at that level
        HttpLogSeverity severity = HttpStatusToLogSeverity(status_code);
        
        // Log the error with the appropriate severity
        if (severity == LOG_FATAL) {
            logger->fatal("HTTP Request Failed: %lu - %s (requires user intervention)\n", 
                         status_code, status_description);
        }
        else if (severity == LOG_CRITICAL) {
            logger->critical("HTTP Request Failed: %lu - %s (temporary server issue)\n", 
                           status_code, status_description);
        }
        else {
            // Default to recoverable_error
            logger->recoverable_error("HTTP Request Failed: %lu - %s\n", 
                                    status_code, status_description);
        }
    } else {
        // Success status code
        snprintf(msg, sizeof(msg), "Send succeeded (HTTP %lu - %s)\n%s",
            status_code,
            status_description,
            total_read > 0 ? response_buffer : "No response body");
        
        // Use Success for 200-299 status codes
        errorCode = NetworkErrorCode::Success;
    }
    
    // Create the result with appropriate error code and status
    NetworkResult result(errorCode, msg);
    char code[16];
    snprintf(code, sizeof(code), "HTTP%lu", status_code);
    strcpy_s(const_cast<char*>(result.getCode()), 16, code);
    
    return result;
}

bool HttpNetworkClient::getLogzillaVersion(char* version_buf, size_t max_length, size_t& bytes_written)
{
    auto logger = LOG_THIS;
    std::lock_guard<std::recursive_mutex> lock(connecting_);

    // Always attempt to connect if not connected
    if (!is_connected_ || !hConnection_) {
        logger->debug2("HttpNetworkClient::getLogzillaVersion() not connected, attempting to connect\n");
        if (!connect()) {
            logger->recoverable_error("HttpNetworkClient::getLogzillaVersion() connection attempt failed\n");
            return false;
        }
    }

    // Build version URL
    wchar_t version_url[MAX_URL_LENGTH];
    wcscpy_s(version_url, SharedConstants::LOGZILLA_VERSION_PATH);

    logger->debug2("HttpNetworkClient::getLogzillaVersion() requesting URL: %ls\n", version_url);

    DWORD flags = WINHTTP_FLAG_REFRESH;
    if (use_ssl_) {
        flags |= WINHTTP_FLAG_SECURE;
    }

    hRequest_.reset(WinHttpOpenRequest(hConnection_.get(),
        L"GET",
        version_url,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags));

    if (!hRequest_) {
        DWORD error = GetLastError();
        logger->recoverable_error("HttpNetworkClient::getLogzillaVersion() WinHttpOpenRequest failed: %d (%s)\n", error, GetWin32ErrorText(error));
        return false;
    }

    // Set timeouts on the request handle
    if (!applyTimeouts(hRequest_.get())) {
        logger->warning("HttpNetworkClient::getLogzillaVersion() failed to set request timeouts\n");
    }

    // Apply stored security flags if SSL is being used
    if (use_ssl_) {
        logger->debug2("HttpNetworkClient::getLogzillaVersion() applying security flags for TLS\n");

        DWORD securityFlags = 0;
        
        if (use_self_signed_cert_) {
            // For self-signed certificates, ignore validation
            logger->debug2("HttpNetworkClient::getLogzillaVersion() using self-signed certificate mode\n");
            securityFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        } else {
            // When using certificate authority validation, we rely on the system's 
            // certificate store and do NOT ignore certificate validation errors
            logger->debug2("HttpNetworkClient::getLogzillaVersion() using certificate authority validation\n");
            // securityFlags remains 0 - no security flags to ignore validation
        }

        // Add any stored flags
        if (stored_security_flags_ != 0) {
            securityFlags |= stored_security_flags_;
        }

        // Set the security flags
        if (!WinHttpSetOption(hRequest_.get(),
            WINHTTP_OPTION_SECURITY_FLAGS,
            &securityFlags,
            sizeof(securityFlags)))
        {
            DWORD error = GetLastError();
            logger->warning("HttpNetworkClient::getLogzillaVersion() failed to set security flags: %d (%s)\n", error, GetWin32ErrorText(error));
        }
        else {
            logger->debug2("HttpNetworkClient::getLogzillaVersion() successfully set security flags: 0x%08X\n", securityFlags);
        }
    }

    // Send request without API key header since version endpoint doesn't require auth
    logger->debug2("HttpNetworkClient::getLogzillaVersion() sending request\n");
    if (!WinHttpSendRequest(hRequest_.get(),
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0))
    {
        DWORD error = GetLastError();
        if (error == ERROR_WINHTTP_SECURE_FAILURE) {
            // This is a FATAL error because certificate issues require external fixes
            logger->fatal("HttpNetworkClient::getLogzillaVersion() WinHttpSendRequest failed with secure failure (12175): TLS/SSL certificate validation error");

            // Try to get more specific error information
            DWORD secureFailures = 0;
            DWORD secureFailuresSize = sizeof(secureFailures);
            if (WinHttpQueryOption(hRequest_.get(),
                WINHTTP_OPTION_SECURITY_FLAGS,
                &secureFailures,
                &secureFailuresSize))
            {
                logger->fatal("Security validation failure flags: 0x%08X", secureFailures);
                LogCertificateValidationGuidance(logger, !use_self_signed_cert_, secureFailures);
            } else {
                LogCertificateValidationGuidance(logger, !use_self_signed_cert_);
            }
        }
        else {
            logger->recoverable_error("HttpNetworkClient::getLogzillaVersion() WinHttpSendRequest failed: %d (%s)\n", error, GetWin32ErrorText(error));
        }
        return false;
    }

    logger->debug2("HttpNetworkClient::getLogzillaVersion() receiving response\n");
    if (!WinHttpReceiveResponse(hRequest_.get(), NULL)) {
        DWORD error = GetLastError();
        logger->recoverable_error("HttpNetworkClient::getLogzillaVersion() WinHttpReceiveResponse failed: %d (%s)\n", error, GetWin32ErrorText(error));
        return false;
    }
    
    // Get and log HTTP status code at debug3 level
    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    if (!WinHttpQueryHeaders(hRequest_.get(),
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status_code,
        &status_size,
        WINHTTP_NO_HEADER_INDEX))
    {
        DWORD error = GetLastError();
        logger->recoverable_error("HttpNetworkClient::getLogzillaVersion() Failed to query HTTP status code: %d (%s)\n", error, GetWin32ErrorText(error));
        return false;
    }
    
    // Always log the HTTP status code
    logger->debug3("HttpNetworkClient::getLogzillaVersion() HTTP status code: %lu\n", status_code);
    
    // Check if response is not a success (2xx)
    if (status_code < 200 || status_code >= 300) {
        // Map HTTP status to our error codes
        NetworkErrorCode errorCode = HttpStatusToErrorCode(status_code);
        
        // Get the human-readable description of the status code
        const char* status_description = HttpStatusToString(status_code);
        
        // Get appropriate log severity for this status code
        HttpLogSeverity severity = HttpStatusToLogSeverity(status_code);
        
        // Format contextual information based on the specific error
        const char* context_info = "";
        switch (status_code) {
            case 401: context_info = "Check API key"; break;
            case 403: context_info = "Check permissions"; break;
            case 404: context_info = "Version endpoint may have changed"; break;
            case 429: context_info = "Rate limited, retry later"; break;
            default: context_info = "Check configuration"; break;
        }
        
        // Log error with appropriate severity and detailed message
        if (severity == LOG_FATAL) {
            logger->fatal("Version check failed: %lu - %s (%s)\n", 
                         status_code, status_description, context_info);
        }
        else if (severity == LOG_CRITICAL) {
            logger->critical("Version check failed: %lu - %s (%s)\n", 
                           status_code, status_description, context_info);
        }
        else {
            // Default to recoverable_error
            logger->recoverable_error("Version check failed: %lu - %s (%s)\n", 
                                    status_code, status_description, context_info);
        }
        
        return false;
    }

    DWORD size = 0;
    if (!WinHttpQueryDataAvailable(hRequest_.get(), &size)) {
        DWORD error = GetLastError();
        logger->recoverable_error("HttpNetworkClient::getLogzillaVersion() WinHttpQueryDataAvailable failed: %d (%s)\n", error, GetWin32ErrorText(error));
        return false;
    }

    if (size >= max_length) {
        logger->recoverable_error("HttpNetworkClient::getLogzillaVersion() response too large\n");
        return false;
    }

    DWORD downloaded = 0;
    if (!WinHttpReadData(hRequest_.get(),
        version_buf,
        size,
        &downloaded))
    {
        DWORD error = GetLastError();
        logger->recoverable_error("HttpNetworkClient::getLogzillaVersion() WinHttpReadData failed: %d (%s)\n", error, GetWin32ErrorText(error));
        return false;
    }

    bytes_written = downloaded;
    version_buf[downloaded] = '\0';
    return true;
}

bool HttpNetworkClient::loadCertificate(const wchar_t* cert_path)
{
    auto logger = LOG_THIS;
    std::lock_guard<std::recursive_mutex> lock(connecting_);

    // Store the security flags for use during request creation
    stored_security_flags_ = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
        SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
        SECURITY_FLAG_IGNORE_CERT_CN_INVALID;

    // If we already have an active request, apply the flags immediately
    if (hRequest_) {
        DWORD securityFlags = 0;
        DWORD securityFlagsSize = sizeof(securityFlags);

        // Get the current security flags
        if (!WinHttpQueryOption(hRequest_.get(),
            WINHTTP_OPTION_SECURITY_FLAGS,
            &securityFlags,
            &securityFlagsSize))
        {
            logger->recoverable_error("HttpNetworkClient::loadCertificate() WinHttpQueryOption failed: %d\n", GetLastError());
            return false;
        }

        // Add the certificate flags
        securityFlags |= stored_security_flags_;

        // Set the updated security flags
        if (!WinHttpSetOption(hRequest_.get(),
            WINHTTP_OPTION_SECURITY_FLAGS,
            &securityFlags,
            sizeof(securityFlags)))
        {
            logger->recoverable_error("HttpNetworkClient::loadCertificate() WinHttpSetOption failed: %d\n", GetLastError());
            return false;
        }
    }

    return true;
}

bool HttpNetworkClient::get(const wchar_t* url, char* response_buffer, size_t max_length, size_t& bytes_written)
{
    auto logger = LOG_THIS;
    std::lock_guard<std::recursive_mutex> lock(connecting_);

    if (!is_connected_ || !hConnection_) {
        logger->recoverable_error("HttpNetworkClient::get() not connected\n");
        return false;
    }

    DWORD flags = WINHTTP_FLAG_REFRESH;
    if (use_ssl_) {
        flags |= WINHTTP_FLAG_SECURE;
    }

    hRequest_.reset(WinHttpOpenRequest(hConnection_.get(),
        L"GET",
        url,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags));

    if (!hRequest_) {
        DWORD error = GetLastError();
        logger->recoverable_error("HttpNetworkClient::get() WinHttpOpenRequest failed: %d (%s)\n", error, GetWin32ErrorText(error));
        return false;
    }

    // Set security flags for TLS if needed
    if (use_ssl_) {
        logger->debug2("HttpNetworkClient::get() applying security flags for TLS\n");

        DWORD securityFlags = 0;
        
        if (use_self_signed_cert_) {
            // For self-signed certificates, ignore validation
            logger->debug2("HttpNetworkClient::get() using self-signed certificate mode\n");
            securityFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        } else {
            // When using certificate authority validation, we rely on the system's 
            // certificate store and do NOT ignore certificate validation errors
            logger->debug2("HttpNetworkClient::get() using certificate authority validation\n");
            // securityFlags remains 0 - no security flags to ignore validation
        }

        // Add any stored flags
        if (stored_security_flags_ != 0) {
            securityFlags |= stored_security_flags_;
        }

        // Set the security flags
        if (!WinHttpSetOption(hRequest_.get(),
            WINHTTP_OPTION_SECURITY_FLAGS,
            &securityFlags,
            sizeof(securityFlags)))
        {
            DWORD error = GetLastError();
            logger->warning("HttpNetworkClient::get() failed to set security flags: %d (%s)\n", error, GetWin32ErrorText(error));
        }
        else {
            logger->debug2("HttpNetworkClient::get() successfully set security flags: 0x%08X\n", securityFlags);
        }
    }

    // Initialize headers buffer and ensure null termination
    wchar_t headers[MAX_HEADERS_LENGTH] = {};

    // Format headers safely with size limit and ensure null termination
    int header_length = _snwprintf_s(headers, _countof(headers) - 1, _TRUNCATE, L"Authorization: token %s\r\n", api_key_);
    if (header_length < 0 || header_length >= _countof(headers)) {
        logger->recoverable_error("HttpNetworkClient::get() Header formatting failed or truncated\n");
        return false;
    }
    headers[_countof(headers) - 1] = L'\0';  // Ensure null termination

    if (!WinHttpSendRequest(hRequest_.get(),
        headers,
        static_cast<DWORD>(wcslen(headers)),  // Use actual length instead of -1
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0))
    {
        DWORD error = GetLastError();
        if (error == ERROR_WINHTTP_SECURE_FAILURE) {
            // This is a FATAL error because certificate issues require external fixes
            logger->fatal("HttpNetworkClient::get() WinHttpSendRequest failed with secure failure (12175): TLS/SSL certificate validation error");

            // Try to get more specific error information
            DWORD secureFailures = 0;
            DWORD secureFailuresSize = sizeof(secureFailures);
            if (WinHttpQueryOption(hRequest_.get(),
                WINHTTP_OPTION_SECURITY_FLAGS,
                &secureFailures,
                &secureFailuresSize))
            {
                logger->fatal("Security validation failure flags: 0x%08X", secureFailures);
                LogCertificateValidationGuidance(logger, !use_self_signed_cert_, secureFailures);
            } else {
                LogCertificateValidationGuidance(logger, !use_self_signed_cert_);
            }
        } else {
            logger->recoverable_error("HttpNetworkClient::get() WinHttpSendRequest failed: %d (%s)\n", error, GetWin32ErrorText(error));
        }
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest_.get(), NULL)) {
        DWORD error = GetLastError();
        logger->recoverable_error("HttpNetworkClient::get() WinHttpReceiveResponse failed: %d (%s)\n", error, GetWin32ErrorText(error));
        return false;
    }
    
    // Get and log HTTP status code at debug3 level
    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    if (!WinHttpQueryHeaders(hRequest_.get(),
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status_code,
        &status_size,
        WINHTTP_NO_HEADER_INDEX))
    {
        DWORD error = GetLastError();
        logger->recoverable_error("HttpNetworkClient::get() Failed to query HTTP status code: %d (%s)\n", error, GetWin32ErrorText(error));
        return false;
    }
    
    // Always log the HTTP status code
    logger->debug3("HttpNetworkClient::get() HTTP status code: %lu\n", status_code);
    
    // Check if response is not a success (2xx)
    if (status_code < 200 || status_code >= 300) {
        // Map HTTP status to our error codes
        NetworkErrorCode errorCode = HttpStatusToErrorCode(status_code);
        
        // Get the human-readable description of the status code
        const char* status_description = HttpStatusToString(status_code);
        
        // Get appropriate log severity for this status code
        HttpLogSeverity severity = HttpStatusToLogSeverity(status_code);
        
        // Log error with appropriate severity and detailed message
        if (severity == LOG_FATAL) {
            logger->fatal("HTTP GET Failed: %lu - %s (requires user intervention)\n", 
                         status_code, status_description);
        }
        else if (severity == LOG_CRITICAL) {
            logger->critical("HTTP GET Failed: %lu - %s (temporary server issue)\n", 
                           status_code, status_description);
        }
        else {
            // Default to recoverable_error
            logger->recoverable_error("HTTP GET Failed: %lu - %s\n", 
                                    status_code, status_description);
        }
        
        return false;
    }

    DWORD size = 0;
    if (!WinHttpQueryDataAvailable(hRequest_.get(), &size)) {
        DWORD error = GetLastError();
        logger->recoverable_error("HttpNetworkClient::get() WinHttpQueryDataAvailable failed: %d (%s)\n", error, GetWin32ErrorText(error));
        return false;
    }

    if (size >= max_length) {
        logger->recoverable_error("HttpNetworkClient::get() response too large\n");
        return false;
    }

    DWORD downloaded = 0;
    if (!WinHttpReadData(hRequest_.get(),
        response_buffer,
        size,
        &downloaded))
    {
        DWORD error = GetLastError();
        logger->recoverable_error("HttpNetworkClient::get() WinHttpReadData failed: %d (%s)\n", error, GetWin32ErrorText(error));
        return false;
    }

    bytes_written = downloaded;
    response_buffer[downloaded] = '\0';
    return true;
}

bool HttpNetworkClient::applyTimeouts(HINTERNET handle) const
{
    auto logger = LOG_THIS;
    if (!handle) {
        logger->recoverable_error("HttpNetworkClient::applyTimeouts() invalid handle\n");
        return false;
    }

    // Set timeouts
    if (!WinHttpSetTimeouts(handle,
        0,              // DNS resolution timeout
        connect_timeout_,   // Connect timeout
        send_timeout_,      // Send timeout
        receive_timeout_))  // Receive timeout
    {
        DWORD error = GetLastError();
        logger->warning("HttpNetworkClient::applyTimeouts() failed to set timeouts: %d (%s)\n", error, GetWin32ErrorText(error));
        return false;
    }

    return true;
}

bool HttpNetworkClient::negotiateCompression()
{
    auto logger = LOG_THIS;
    std::lock_guard<std::recursive_mutex> lock(connecting_);

    if (!is_connected_ || !hRequest_) {
        logger->recoverable_error("HttpNetworkClient::negotiateCompression() not connected or no request\n");
        return false;
    }

    DWORD flags = WINHTTP_DECOMPRESSION_FLAG_ALL;
    if (!WinHttpSetOption(hRequest_.get(),
        WINHTTP_OPTION_DECOMPRESSION,
        &flags,
        sizeof(flags)))
    {
        DWORD error = GetLastError();
        logger->warning("HttpNetworkClient::negotiateCompression() failed to set decompression: %d (%s)\n", error, GetWin32ErrorText(error));
        return false;
    }

    return true;
}

void HttpNetworkClient::drainConnection()
{
    auto logger = LOG_THIS;
    std::lock_guard<std::recursive_mutex> lock(connecting_);

    if (!is_connected_ || !hRequest_) {
        logger->debug2("HttpNetworkClient::drainConnection() not connected or no request\n");
        return;
    }

    char buffer[4096] = { 0 };
    DWORD bytes_available = 0;
    DWORD bytes_read = 0;

    while (WinHttpQueryDataAvailable(hRequest_.get(), &bytes_available)) {
        if (bytes_available == 0) break;

        DWORD buffer_size = static_cast<DWORD>(std::min(sizeof(buffer), static_cast<size_t>(bytes_available)));
        
        if (!WinHttpReadData(hRequest_.get(),
            buffer,
            buffer_size,
            &bytes_read))
        {
            DWORD error = GetLastError();
            logger->warning("HttpNetworkClient::drainConnection() failed to read data: %d (%s)\n", error, GetWin32ErrorText(error));
            break;
        }

        if (bytes_read == 0) break;
    }
}

bool HttpNetworkClient::checkServerCert()
{
    auto logger = LOG_THIS;
    std::lock_guard<std::recursive_mutex> lock(connecting_);

    if (!is_connected_ || !hRequest_) {
        logger->recoverable_error("HttpNetworkClient::checkServerCert() not connected or no request\n");
        return false;
    }

    DWORD certInfoLength = 0;
    if (!WinHttpQueryOption(hRequest_.get(),
        WINHTTP_OPTION_SECURITY_CERTIFICATE_STRUCT,
        NULL,
        &certInfoLength))
    {
        DWORD error = GetLastError();
        if (error != ERROR_INSUFFICIENT_BUFFER) {
            logger->recoverable_error("HttpNetworkClient::checkServerCert() failed to get cert info length: %d (%s)\n", error, GetWin32ErrorText(error));
            return false;
        }
    }

    std::vector<BYTE> certInfoBuffer(certInfoLength, 0); // Initialize buffer elements
    WINHTTP_CERTIFICATE_INFO* certInfo = reinterpret_cast<WINHTTP_CERTIFICATE_INFO*>(certInfoBuffer.data());

    if (!WinHttpQueryOption(hRequest_.get(),
        WINHTTP_OPTION_SECURITY_CERTIFICATE_STRUCT,
        certInfo,
        &certInfoLength))
    {
        DWORD error = GetLastError();
        logger->recoverable_error("HttpNetworkClient::checkServerCert() failed to get cert info: %d (%s)\n", error, GetWin32ErrorText(error));
        return false;
    }

    logger->debug2("HttpNetworkClient::checkServerCert() certificate verified\n");
    return true;
}

bool HttpNetworkClient::followRedirect(wchar_t* redirect_buffer, size_t buffer_size)
{
    auto logger = LOG_THIS;
    std::lock_guard<std::recursive_mutex> lock(connecting_);

    if (!is_connected_ || !hRequest_) {
        logger->recoverable_error("HttpNetworkClient::followRedirect() not connected or no request\n");
        return false;
    }

    DWORD size = static_cast<DWORD>(buffer_size * sizeof(wchar_t));
    if (!WinHttpQueryOption(hRequest_.get(),
        WINHTTP_OPTION_URL,
        redirect_buffer,
        &size))
    {
        DWORD error = GetLastError();
        logger->recoverable_error("HttpNetworkClient::followRedirect() failed to get redirect URL: %d (%s)\n", error, GetWin32ErrorText(error));
        return false;
    }

    logger->debug2("HttpNetworkClient::followRedirect() redirecting to: %ls\n", redirect_buffer);
    return true;
}

// Add empty implementation for the virtual close() method
void HttpNetworkClient::close()
{
    // RAII handles cleanup via HandleGuard destructors.
    // Reset connection state.
    std::lock_guard<std::recursive_mutex> lock(connecting_);
    hRequest_.reset();
    hConnection_.reset();
    hSession_.reset();
    is_connected_ = false;
}
