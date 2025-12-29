#include "pch.h"
#include <memory>
#include <stdio.h>
#include <Windows.h>
#include <winevt.h>
#include <string>
#include <regex>
#include <winhttp.h>
#include <wincrypt.h>

#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")

// Struct to handle URL parsing
struct UrlComponents {
    std::wstring host;
    INTERNET_PORT port;
    std::wstring path;
    bool is_ssl;
};

// Struct to manage certificate contexts
struct CertificateContext {
    PCCERT_CONTEXT pCertContext;
    HCERTSTORE hCertStore;
    BYTE* pfxBuffer;

    CertificateContext() : pCertContext(NULL), hCertStore(NULL), pfxBuffer(NULL) {}
    ~CertificateContext() {
        if (pCertContext) CertFreeCertificateContext(pCertContext);
        if (hCertStore) CertCloseStore(hCertStore, 0);
        if (pfxBuffer) delete[] pfxBuffer;
    }
};

// Helper function declarations
int ConvertToUtf8(const wchar_t* wide_text, char* utf8_text, int output_buffer_size);
int setErrorMessage(char* error_message_buffer, int error_message_buffer_size, const wchar_t* format, ...);
DWORD GetChannelNamesInternal(char* output_buffer, int output_buffer_size, char* error_message_buffer, int error_message_buffer_size);
DWORD IsChannelDisabledInternal(const wchar_t* channel_name);

// Helper function to validate API key format
// Helper function to validate API key format
bool IsValidApiKeyFormat(const wchar_t* api_key) {
    if (!api_key || wcslen(api_key) < 48 || wcslen(api_key) > 54) return false;

    std::wregex pattern(L"^[a-zA-Z0-9-]{48,54}$");  // Added hyphen to character class
    return std::regex_match(api_key, pattern);
}

// Parse URL into components including optional port
bool ParseUrl(const wchar_t* url, UrlComponents& components) {
    std::wstring wurl(url);

    // Default values
    components.port = 0;
    components.is_ssl = false;
    components.path = L"/api/";

    // Check for HTTP/HTTPS prefix
    if (wurl.find(L"https://") == 0) {
        components.is_ssl = true;
        wurl = wurl.substr(8);
    }
    else if (wurl.find(L"http://") == 0) {
        wurl = wurl.substr(7);
    }

    // Find port and path separators
    size_t portPos = wurl.find(L':');
    size_t pathPos = wurl.find(L'/');

    // Extract hostname
    if (portPos != std::wstring::npos) {
        components.host = wurl.substr(0, portPos);
    }
    else if (pathPos != std::wstring::npos) {
        components.host = wurl.substr(0, pathPos);
    }
    else {
        components.host = wurl;
    }

    // Extract port if specified
    if (portPos != std::wstring::npos) {
        std::wstring portStr;
        if (pathPos != std::wstring::npos) {
            portStr = wurl.substr(portPos + 1, pathPos - portPos - 1);
        }
        else {
            portStr = wurl.substr(portPos + 1);
        }

        try {
            components.port = static_cast<INTERNET_PORT>(_wtoi(portStr.c_str()));
            if (components.port == 0) {
                return false; // Invalid port
            }
        }
        catch (...) {
            return false;
        }
    }

    // Set default ports if none specified
    if (components.port == 0) {
        components.port = components.is_ssl ?
            INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    }

    // Extract path if present
    if (pathPos != std::wstring::npos) {
        components.path = wurl.substr(pathPos);
    }

    return true;
}

bool LoadCertificate(const wchar_t* cert_path, CertificateContext& cert_ctx) {
    // Open the PFX file
    HANDLE hFile = CreateFile(cert_path, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD dwFileSize = GetFileSize(hFile, NULL);
    cert_ctx.pfxBuffer = new BYTE[dwFileSize];

    // Read the file
    DWORD dwRead = 0;
    if (!ReadFile(hFile, cert_ctx.pfxBuffer, dwFileSize, &dwRead, NULL)) {
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);

    // Import the certificate
    CRYPT_DATA_BLOB pfxBlob;
    pfxBlob.pbData = cert_ctx.pfxBuffer;
    pfxBlob.cbData = dwFileSize;

    cert_ctx.hCertStore = PFXImportCertStore(&pfxBlob, L"", 0);
    if (!cert_ctx.hCertStore) return false;

    cert_ctx.pCertContext = CertFindCertificateInStore(
        cert_ctx.hCertStore,
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        0,
        CERT_FIND_ANY,
        NULL,
        NULL);

    return cert_ctx.pCertContext != NULL;
}

DWORD ValidateServerCertificate(HINTERNET hRequest, CertificateContext& cert_ctx) {
    PCCERT_CONTEXT pServerCert = NULL;
    DWORD dwSize = sizeof(pServerCert);

    if (!WinHttpQueryOption(hRequest, WINHTTP_OPTION_SERVER_CERT_CONTEXT,
        &pServerCert, &dwSize) || !pServerCert) {
        return GetLastError();
    }

    // Compare certificate hashes
    BYTE localHash[20], serverHash[20];
    DWORD hashSize = sizeof(localHash);

    bool isValid = CertGetCertificateContextProperty(cert_ctx.pCertContext,
        CERT_SHA1_HASH_PROP_ID, localHash, &hashSize) &&
        CertGetCertificateContextProperty(pServerCert,
            CERT_SHA1_HASH_PROP_ID, serverHash, &hashSize) &&
        memcmp(localHash, serverHash, hashSize) == 0;

    CertFreeCertificateContext(pServerCert);
    return isValid ? ERROR_SUCCESS : ERROR_INVALID_DATA;
}

extern "C" {
    // Original exports
    __declspec(dllexport) DWORD GetChannelNames(unsigned char* output_buffer,
        int output_buffer_size, unsigned char* error_message_buffer,
        int error_message_buffer_size);

    __declspec(dllexport) DWORD IsChannelDisabled(const wchar_t* channel_name);

    // Add ValidateApiKey declaration here
    __declspec(dllexport) DWORD ValidateApiKey(
        const wchar_t* url,
        const wchar_t* api_key,
        const wchar_t* cert_path,
        char* error_message_buffer,
        int error_message_buffer_size);
}

// Original exports implementations
DWORD GetChannelNames(unsigned char* output_buffer,
    int output_buffer_size, unsigned char* error_message_buffer,
    int error_message_buffer_size) {
    return GetChannelNamesInternal(
        (char*)output_buffer,
        output_buffer_size,
        (char*)error_message_buffer,
        error_message_buffer_size);
}

DWORD IsChannelDisabled(const wchar_t* channel_name) {
    return IsChannelDisabledInternal(channel_name);
}

// ValidateApiKey implementation outside the extern block
DWORD ValidateApiKey(
    const wchar_t* url,
    const wchar_t* api_key,
    const wchar_t* cert_path,
    char* error_message_buffer,
    int error_message_buffer_size)
{
    // Validate input parameters
    if (!url || !api_key) {
        setErrorMessage(error_message_buffer, error_message_buffer_size,
            L"URL or API key is null");
        return ERROR_INVALID_PARAMETER;
    }

    // Validate API key format
    if (!IsValidApiKeyFormat(api_key)) {
        setErrorMessage(error_message_buffer, error_message_buffer_size,
            L"Invalid API key format");
        return ERROR_INVALID_DATA;
    }

    // Parse URL
    UrlComponents urlComponents;
    if (!ParseUrl(url, urlComponents)) {
        setErrorMessage(error_message_buffer, error_message_buffer_size,
            L"Invalid URL format");
        return ERROR_INVALID_PARAMETER;
    }

    // Initialize WinHTTP
    HINTERNET hSession = WinHttpOpen(L"SyslogAgent/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        DWORD error = GetLastError();
        setErrorMessage(error_message_buffer, error_message_buffer_size,
            L"Failed to initialize HTTP session: %u", error);
        return error;
    }

    // Set timeouts (all values in milliseconds)
    DWORD timeoutMs = 30 * 1000; // 30 seconds
    WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    // Connect using parsed components
    HINTERNET hConnect = WinHttpConnect(hSession,
        urlComponents.host.c_str(),
        urlComponents.port, 0);
    if (!hConnect) {
        DWORD error = GetLastError();
        setErrorMessage(error_message_buffer, error_message_buffer_size,
            L"Failed to connect to %s:%d - Error: %u",
            urlComponents.host.c_str(), urlComponents.port, error);
        WinHttpCloseHandle(hSession);
        return error;
    }

    // Create the request
    DWORD flags = urlComponents.is_ssl ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
        urlComponents.path.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        DWORD error = GetLastError();
        setErrorMessage(error_message_buffer, error_message_buffer_size,
            L"Failed to create HTTP request: %u", error);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return error;
    }

    // Handle SSL if needed
    CertificateContext cert_ctx;
    if (urlComponents.is_ssl) {
        if (cert_path == NULL) {
            setErrorMessage(error_message_buffer, error_message_buffer_size,
                L"Certificate path required for HTTPS but not provided");
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return ERROR_FILE_NOT_FOUND;
        }

        if (!LoadCertificate(cert_path, cert_ctx)) {
            setErrorMessage(error_message_buffer, error_message_buffer_size,
                L"Failed to load certificate");
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return ERROR_FILE_NOT_FOUND;
        }

        DWORD dwFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA
            | SECURITY_FLAG_IGNORE_CERT_CN_INVALID
            | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS,
            &dwFlags, sizeof(dwFlags));
    }

    // Add headers
    std::wstring headers = L"Authorization: token ";
    headers += api_key;
    headers += L"\r\nContent-Type: application/json\r\n";

    if (!WinHttpAddRequestHeaders(hRequest, headers.c_str(), -1L,
        WINHTTP_ADDREQ_FLAG_ADD)) {
        DWORD error = GetLastError();
        setErrorMessage(error_message_buffer, error_message_buffer_size,
            L"Failed to add request headers: %u", error);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return error;
    }

    // Send request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        DWORD error = GetLastError();
        switch (error) {
        case ERROR_WINHTTP_TIMEOUT:
            setErrorMessage(error_message_buffer, error_message_buffer_size,
                L"Connection timed out while sending request");
            break;
        case ERROR_WINHTTP_CANNOT_CONNECT:
            setErrorMessage(error_message_buffer, error_message_buffer_size,
                L"Failed to connect to server %s:%d",
                urlComponents.host.c_str(), urlComponents.port);
            break;
        case ERROR_WINHTTP_NAME_NOT_RESOLVED:
            setErrorMessage(error_message_buffer, error_message_buffer_size,
                L"Could not resolve server name: %s",
                urlComponents.host.c_str());
            break;
        default:
            setErrorMessage(error_message_buffer, error_message_buffer_size,
                L"Failed to send request: %u", error);
        }
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return error;
    }

    // Get response
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD error = GetLastError();
        if (error == ERROR_WINHTTP_TIMEOUT) {
            setErrorMessage(error_message_buffer, error_message_buffer_size,
                L"Connection timed out while waiting for response");
        }
        else {
            setErrorMessage(error_message_buffer, error_message_buffer_size,
                L"Failed to receive response: %u", error);
        }
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return error;
    }

    // Validate SSL certificate if using TLS
    if (urlComponents.is_ssl) {
        DWORD certResult = ValidateServerCertificate(hRequest, cert_ctx);
        if (certResult != ERROR_SUCCESS) {
            setErrorMessage(error_message_buffer, error_message_buffer_size,
                L"Certificate validation failed");
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return certResult;
        }
    }

    // Check status code
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
        WINHTTP_NO_HEADER_INDEX)) {
        DWORD error = GetLastError();
        setErrorMessage(error_message_buffer, error_message_buffer_size,
            L"Failed to query response status: %u", error);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return error;
    }

    // Clean up
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Return appropriate status
    switch (statusCode) {
    case 200:
        return ERROR_SUCCESS;
    case 401:
        setErrorMessage(error_message_buffer, error_message_buffer_size,
            L"Invalid API key");
        return ERROR_INVALID_ACCESS;
    case 403:
        setErrorMessage(error_message_buffer, error_message_buffer_size,
            L"Access forbidden");
        return ERROR_ACCESS_DENIED;
    case 404:
        setErrorMessage(error_message_buffer, error_message_buffer_size,
            L"API endpoint not found (404)");
        return ERROR_PATH_NOT_FOUND;
    case 0:
        setErrorMessage(error_message_buffer, error_message_buffer_size,
            L"No response from server");
        return ERROR_NO_DATA;
    default:
        setErrorMessage(error_message_buffer, error_message_buffer_size,
            L"Server returned unexpected status code: %d", statusCode);
        return ERROR_INVALID_FUNCTION;
    }
}

int ConvertToUtf8(const wchar_t* wide_text, char* utf8_text,
    int output_buffer_size) {
    return WideCharToMultiByte(CP_UTF8, 0, wide_text, -1, utf8_text,
        output_buffer_size, NULL, NULL);
}

DWORD GetChannelNamesInternal(char* output_buffer, int output_buffer_size,
    char* error_message_buffer, int error_message_buffer_size);
DWORD IsChannelDisabledInternal(const wchar_t* channel_name);

int setErrorMessage(char* error_message_buffer, int error_message_buffer_size,
    const wchar_t* format, ...) {
    wchar_t* temp_buffer = (wchar_t*) new wchar_t[error_message_buffer_size];
    va_list args;
    va_start(args, format);
    int result = _vsnwprintf_s(
        (wchar_t*)temp_buffer,
        (size_t)error_message_buffer_size,
        _TRUNCATE,
        format,
        args
    );
    va_end(args);
    if (result == 0)
        return 0;
    result = ConvertToUtf8(temp_buffer, error_message_buffer, error_message_buffer_size);
    delete temp_buffer;
    return result;
}

DWORD GetChannelNamesInternal(char* output_buffer, int output_buffer_size,
    char* error_message_buffer, int error_message_buffer_size)
{
    static const int UTF8_BUFFER_SIZE = 1000;
    EVT_HANDLE hChannels = NULL;
    LPWSTR pBuffer = NULL;
    LPWSTR pTemp = NULL;
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;
    DWORD status = ERROR_SUCCESS;

    error_message_buffer[0] = 0;

    int output_buffer_idx = 0;
    // Get a handle to an enumerator that contains all the names of the 
    // channels registered on the computer.
    hChannels = EvtOpenChannelEnum(NULL, 0);

    if (NULL == hChannels)
    {
        status = GetLastError();
        setErrorMessage(error_message_buffer, error_message_buffer_size,
            L"(%d) EvtOpenChannelEnum failed", status);
        goto cleanup;
    }

    // Enumerate through the list of channel names. If the buffer is not big
    // enough reallocate the buffer. To get the configuration information for
    // a channel, call the EvtOpenChannelConfig function.
    while (true)
    {
        if (!EvtNextChannelPath(hChannels, dwBufferSize, pBuffer, &dwBufferUsed))
        {
            status = GetLastError();

            if (ERROR_NO_MORE_ITEMS == status)
            {
                break;
            }
            else if (ERROR_INSUFFICIENT_BUFFER == status)
            {
                dwBufferSize = dwBufferUsed;
                pTemp = (LPWSTR)realloc(pBuffer, dwBufferSize * sizeof(WCHAR));
                if (pTemp)
                {
                    pBuffer = pTemp;
                    pTemp = NULL;
                    EvtNextChannelPath(hChannels, dwBufferSize, pBuffer, &dwBufferUsed);
                }
                else
                {
                    status = ERROR_OUTOFMEMORY;
                    setErrorMessage(error_message_buffer, error_message_buffer_size,
                        L"(%d) realloc failed", status);
                    goto cleanup;
                }
            }
            else
            {
                setErrorMessage(error_message_buffer, error_message_buffer_size,
                    L"(%d) EvtNextChannelPath failed", status);
                break;
            }
        }

        if (output_buffer_idx + dwBufferUsed > output_buffer_size - 1) {
            setErrorMessage(error_message_buffer, error_message_buffer_size,
                L"(%d) Buffer too small", ERROR_INSUFFICIENT_BUFFER);
            break;
        }
        else {
            int num_chars = ConvertToUtf8(pBuffer, output_buffer + output_buffer_idx,
                output_buffer_size - output_buffer_idx);
            output_buffer_idx += num_chars;
        }
    }

cleanup:
    if (hChannels)
        EvtClose(hChannels);

    if (pBuffer)
        free(pBuffer);

    return output_buffer_idx;
}

DWORD IsChannelDisabledInternal(const wchar_t* channel_name) {
    // returns 0 if channel enabled, ~0 if disabled, otherwise error code
    EVT_HANDLE hChannel = NULL;
    DWORD status = ERROR_SUCCESS;
    PEVT_VARIANT pProperty = NULL;  // Buffer that receives the property value
    PEVT_VARIANT pTemp = NULL;
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;


    hChannel = EvtOpenChannelConfig(NULL, channel_name, 0);

    if (NULL == hChannel) // Fails with 15007 (ERROR_EVT_CHANNEL_NOT_FOUND) 
        // if the channel is not found
    {
        auto status = GetLastError();
        if (status == ERROR_SUCCESS) {
            // this didn't work, can't return that
            return ERROR_INVALID_TARGET_HANDLE;
        }
        return status;
        goto cleanup;
    }

    if (!EvtGetChannelConfigProperty(hChannel, EvtChannelConfigEnabled, 0,
        dwBufferSize, pProperty, &dwBufferUsed))
    {
        status = GetLastError();
        if (ERROR_INSUFFICIENT_BUFFER == status)
        {
            dwBufferSize = dwBufferUsed;
            pTemp = (PEVT_VARIANT)realloc(pProperty, dwBufferSize);
            if (pTemp)
            {
                pProperty = pTemp;
                pTemp = NULL;
                if (!EvtGetChannelConfigProperty(hChannel, EvtChannelConfigEnabled,
                    0, dwBufferSize, pProperty, &dwBufferUsed)) {
                    status = GetLastError();
                }
                else {
                    status = ERROR_SUCCESS;
                }
            }
            else
            {
                status = ERROR_OUTOFMEMORY;
                goto cleanup;
            }
        }
    }
    if (ERROR_SUCCESS == status)
    {
        if (pProperty != NULL && pProperty->BooleanVal) {
            status = 0;
        }
        else {
            status = ~0;
        }
    }

cleanup:

    if (hChannel)
        EvtClose(hChannel);

    return status;
}
