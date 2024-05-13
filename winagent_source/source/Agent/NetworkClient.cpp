/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#include "stdafx.h"
#include "Windows.h"
#include "wincrypt.h"
#include "WinHttp.h"
#include <limits>
#include <regex>
#include <string>
#include "Logger.h"
#include "NetworkClient.h"
#include "SyslogAgentSharedConstants.h"
#include "Util.h"
#undef min // to allow std::min

#pragma comment(lib, "winhttp.lib")
#pragma comment (lib, "crypt32")



namespace Syslog_agent {

    NetworkClient::~NetworkClient()
    {
        if (hConnect_) {
            WinHttpCloseHandle(hConnect_);
            hConnect_ = NULL;
        }
        if (use_ssl_)
        {
            if (pCertContext_ != NULL)
            {
                CertFreeCertificateContext(pCertContext_);
            }
            if (hCertStore_ != NULL)
            {
                CertCloseStore(hCertStore_, 0);
            }

            // Don't forget to free the memory allocated for the PFX file
            if (pfxBuffer_ != NULL) {
                delete[] pfxBuffer_;
            }

        }
        if (hSession_) {
            WinHttpCloseHandle(hSession_);
        }
    }


    bool NetworkClient::initialize(const Configuration* config, const wstring api_key, 
        const std::wstring& url, bool use_ssl, unsigned int port)
    {
        config_ = config;
        api_key_ = api_key;
        // Regular expression to match URL and optional port (e.g., "http://127.0.0.1:5000")
        std::wregex url_regex(LR"(^(http:\/\/|https:\/\/)?([^\/:]+)(:\d+)?(\/.*)?$)");
        std::wsmatch matches;

        if (std::regex_search(url, matches, url_regex))
        {
            if (matches.size() >= 3)
            {
                // URL without port
                url_ = matches[2].str();

                if (matches.size() >= 4 && matches[3].matched)
                {
                    // Extract port number
                    std::wstring port_str = matches[3].str().substr(1); // Remove ':' from port
                    port_ = std::stoi(port_str);
                }
                else
                {
                    // Use default port if no port is specified in the URL
                    port_ = ((port != 0) ? port // Use port from parameter if specified)
                        : (use_ssl ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT));
                }
            }
            else
            {
                // Invalid URL format
                Logger::critical("NetworkClient::Initialize()> Invalid URL format.\n");
                return false;
            }
        }
        else
        {
            // Regex matching failed
            Logger::critical("NetworkClient::Initialize()> URL parsing failed.\n");
            return false;
        }

        // Rest of the initialization code...
        hSession_ = WinHttpOpen(SharedConstants::USER_AGENT,
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);

        if (!hSession_)
        {
            Logger::critical("NetworkClient::Initialize()> Error %u in WinHttpOpen.\n",
                GetLastError());
            return false;
        }

        use_ssl_ = use_ssl;
        hConnect_ = NULL;
        hRequest_ = NULL;

        return true;

    }


    bool NetworkClient::initialize(const Configuration* config, const wstring api_key, 
        const std::wstring& url) {
        // Determine use_ssl based on URL prefix
        bool use_ssl = url.find(L"https://") == 0;

        // Call the original initialize method with determined use_ssl
        // Default port will be set in the original initialize method
        return initialize(config, api_key, url, use_ssl);
    }

    bool NetworkClient::loadCertificate(const std::wstring& cert_path)
    {
        // Open the PFX file
        HANDLE hFile = CreateFile(L"E:\\Source\\Mine\\Logzilla\\syslogagent\\syslogagent\\"
            "source\\Config\\bin\\x64\\Debug\\primary.pfx", 
            GENERIC_READ, 
            FILE_SHARE_READ, 
            NULL, 
            OPEN_EXISTING, 
            FILE_ATTRIBUTE_NORMAL, 
            NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            Logger::critical("NetworkClient::LoadCertificate()> Error %u in CreateFile.\n",
                GetLastError());
            return false;
        }
        DWORD dwFileSize = GetFileSize(hFile, NULL);
        pfxBuffer_ = new BYTE[dwFileSize];

        // Read the file into memory
        DWORD dwRead = 0;
        if (!ReadFile(hFile, pfxBuffer_, dwFileSize, &dwRead, NULL)) {
            Logger::critical("NetworkClient::LoadCertificate()> Error %u in ReadFile.\n",
                GetLastError());
            return false;
        }

        // Close the file
        CloseHandle(hFile);

        CRYPT_DATA_BLOB pfxBlob;
        pfxBlob.pbData = pfxBuffer_;
        pfxBlob.cbData = dwFileSize;

        // hCertStore_ = PFXImportCertStore(&pfxBlob, L"pfx-password", 0);
        hCertStore_ = PFXImportCertStore(&pfxBlob, L"", 0);
        if (!hCertStore_) {
            Logger::critical("NetworkClient::LoadCertificate()> Error %u in"
                " PFXImportCertStore.\n",
                GetLastError());
            return false;
        }

        pCertContext_ = CertFindCertificateInStore(
            hCertStore_, 
            X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 
            0, 
            CERT_FIND_ANY, 
            NULL, 
            NULL);
        if (!pCertContext_) {
            Logger::critical("NetworkClient::LoadCertificate()> Error %u in"
                " CertFindCertificateInStore.\n",
                GetLastError());
            return false;
        }
        return true;

    }


    bool NetworkClient::connect()
    {
        if (use_ssl_)
        {
            hConnect_ = WinHttpConnect(hSession_, url_.c_str(), port_, 0);
        }
        else
        {
            hConnect_ = WinHttpConnect(hSession_, url_.c_str(), port_, 0);
        }

        if (!hConnect_)
        {
            Logger::recoverable_error("NetworkClient::Connect()> Error %u in"
                " WinHttpConnect.\n", GetLastError());
            server_cert_checked_ = false;
            return false;
        }

        if (use_ssl_ && server_cert_checked_ == false) {
            if (!checkServerCert()) {
                Logger::critical("NetworkClient::Connect()> Server cert check"
                    " failed! Error %u.\n", GetLastError());
                return false;
            }
            server_cert_checked_ = true;
        }

        return true;
    }


    NetworkClient::RESULT_TYPE NetworkClient::post(const char* buf, 
        uint32_t length) {
        // Ensure connection is available
        if (!hConnect_) {
            Logger::recoverable_error("NetworkClient::post()> No "
                " connection available.\n");
            return ERROR_NOT_CONNECTED; // Ensure this constant is 
                                        // defined appropriately
        }

        // Setup the request with or without secure flags based 
        // on SSL usage
        DWORD flags = use_ssl_ ? WINHTTP_FLAG_SECURE : 0;
        hRequest_ = WinHttpOpenRequest(hConnect_, L"POST", 
            config_->api_path_.c_str(),
            NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 
            flags);
        if (!hRequest_) {
            DWORD error = GetLastError();
            Logger::recoverable_error("NetworkClient::Post()> Error"
                " %u in WinHttpOpenRequest.\n", error);
            return error;
        }

        if (use_ssl_) {
            DWORD dwFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA 
                | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE 
                | SECURITY_FLAG_IGNORE_CERT_CN_INVALID 
                | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
            if (!WinHttpSetOption(hRequest_, 
                WINHTTP_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags))) {
                DWORD error = GetLastError();
                Logger::recoverable_error("NetworkClient::post()>"
                    " Error %u in WinHttpSetOption.\n", error);
                WinHttpCloseHandle(hRequest_);
                hRequest_ = NULL;  // Ensures we do not use an invalid handle
                return error;
            }
        }

        // Construct headers
        std::wstring headers = L"Authorization: token " + api_key_ 
            + L"\r\n" L"Content-Type: application/json\r\n";
        if (!WinHttpAddRequestHeaders(hRequest_, headers.c_str(), 
            (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD)) {
            DWORD error = GetLastError();
            Logger::recoverable_error("NetworkClient::Post()> Error"
                " %u in WinHttpAddRequestHeaders.\n", error);
            WinHttpCloseHandle(hRequest_);
            hRequest_ = NULL;
            return error;
        }

        // Send the POST request
        if (!WinHttpSendRequest(hRequest_, WINHTTP_NO_ADDITIONAL_HEADERS, 
            0, (LPVOID)buf, length, length, 0)) {
            DWORD error = GetLastError();
            Logger::recoverable_error("NetworkClient::Post()> Error %u"
                " in WinHttpSendRequest.\n", error);
            WinHttpCloseHandle(hRequest_);
            hRequest_ = NULL;
            return error;
        }

        // Wait for the response
        if (!WinHttpReceiveResponse(hRequest_, NULL)) {
            DWORD error = GetLastError();
            Logger::recoverable_error("NetworkClient::Post()> Error"
                " %u in WinHttpReceiveResponse.\n", error);
            WinHttpCloseHandle(hRequest_);
            hRequest_ = NULL;
            return error;
        }

        // Check the HTTP status code
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        if (!WinHttpQueryHeaders(hRequest_, WINHTTP_QUERY_STATUS_CODE 
            | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, 
            WINHTTP_NO_HEADER_INDEX)) {
            DWORD error = GetLastError();
            Logger::recoverable_error("NetworkClient::Post()> Error"
                " %u in WinHttpQueryHeaders.\n", error);
            WinHttpCloseHandle(hRequest_);
            hRequest_ = NULL;
            return error;
        }

        // Handle different status codes
        if (dwStatusCode >= 200 && dwStatusCode <= 299) {
            WinHttpCloseHandle(hRequest_);
            hRequest_ = NULL;
            return RESULT_SUCCESS;
        }
        else if (dwStatusCode == 401)
        {
            Logger::fatal("NetworkClient::Post()> Wrong API key"
                " (HTTP status code %u).\n",
                dwStatusCode);
            return dwStatusCode;
        }
        else if (dwStatusCode == 403)
        {
            // Handle forbidden case
            Logger::fatal("NetworkClient::Post()> Access forbidden,"
                " check API key (HTTP status code %u).\n", dwStatusCode);
            return dwStatusCode;
        }
        else
        {
            // Error, you can handle specific status codes as needed
            Logger::recoverable_error("NetworkClient::Post()> Error:"
                " received HTTP status code %u.\n", dwStatusCode);
            WinHttpCloseHandle(hRequest_);
            hRequest_ = NULL;
            return dwStatusCode;
        }
    }


    NetworkClient::RESULT_TYPE NetworkClient::post(const std::string& data)
    {
        return post(data.c_str(), static_cast<uint32_t>(data.length()));
    }

    NetworkClient::RESULT_TYPE NetworkClient::post(const std::wstring& data) {
        string s = Util::wstr2str(data);
        return post(s);
    }


    bool NetworkClient::readResponse(std::string& response) const
    {
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        LPSTR pszOutBuffer;
        BOOL  bResults = FALSE;
        DWORD dwBytesRead = 0;

        do
        {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest_, &dwSize))
            {
                Logger::critical("NetworkClient::ReadResponse()> Error"
                    " %u in WinHttpQueryDataAvailable.\n",
                    GetLastError());
                return false;
            }

            pszOutBuffer = new char[dwSize + 1];
            if (!pszOutBuffer)
            {
                Logger::critical("NetworkClient::ReadResponse()> Out of"
                    " memory\n");
                dwSize = 0;
                return false;
            }

            ZeroMemory(pszOutBuffer, dwSize + 1);

            if (!WinHttpReadData(hRequest_, (LPVOID)pszOutBuffer,
                dwSize, &dwDownloaded))
            {
                Logger::critical("NetworkClient::ReadResponse()> Error"
                    " %u in WinHttpReadData.\n", GetLastError());
                return false;
            }
            else
            {
                response.append(pszOutBuffer, dwDownloaded);
            }

            delete[] pszOutBuffer;

        } while (dwSize > 0);

        return true;
    }


    void NetworkClient::close() {
        if (hRequest_) {
            WinHttpCloseHandle(hRequest_);
            hRequest_ = NULL;
        }
        if (hConnect_) {
            WinHttpCloseHandle(hConnect_);
            hConnect_ = NULL;
        }
    }

    
    bool NetworkClient::checkServerCert() {
        if (!use_ssl_) {
            return true;
        }

        hRequest_ = WinHttpOpenRequest(hConnect_, L"GET", L"/",
            NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 
            WINHTTP_FLAG_SECURE);

        if (!hRequest_) {
            Logger::critical("NetworkClient::checkServerCert()> Error"
                " %u in WinHttpOpenRequest.\n", GetLastError());
            return false;
        }

        DWORD dwFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA 
            | SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        if (!WinHttpSetOption(hRequest_, WINHTTP_OPTION_SECURITY_FLAGS, 
            &dwFlags, sizeof(dwFlags))) {
            Logger::critical("NetworkClient::checkServerCert()> Error"
                " %u in WinHttpSetOption.\n", GetLastError());
            WinHttpCloseHandle(hRequest_);
            hRequest_ = NULL;
            return false;
        }

        if (!WinHttpSendRequest(hRequest_, WINHTTP_NO_ADDITIONAL_HEADERS, 
            0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            Logger::critical("NetworkClient::checkServerCert()> Error"
                " %u in WinHttpSendRequest.\n", GetLastError());
            WinHttpCloseHandle(hRequest_);
            hRequest_ = NULL;
            return false;
        }

        if (!WinHttpReceiveResponse(hRequest_, NULL)) {
            Logger::critical("NetworkClient::checkServerCert()> Error"
                " %u in WinHttpReceiveResponse.\n", GetLastError());
            WinHttpCloseHandle(hRequest_);
            hRequest_ = NULL;
            return false;
        }

        PCCERT_CONTEXT pServerCert = NULL;
        DWORD dwSize = sizeof(pServerCert);

        if (!WinHttpQueryOption(hRequest_, WINHTTP_OPTION_SERVER_CERT_CONTEXT, 
            &pServerCert, &dwSize) || !pServerCert) {
            Logger::critical("NetworkClient::checkServerCert()> Failed to get"
                " server certificate, Error %u.\n", GetLastError());
            if (pServerCert) CertFreeCertificateContext(pServerCert);
            WinHttpCloseHandle(hRequest_);
            hRequest_ = NULL;
            return false;
        }

        // Assuming further use of pServerCert is valid here
        BYTE hash[20]; // SHA-1 hash size
        DWORD hashSize = sizeof(hash);
        if (!CertGetCertificateContextProperty(pServerCert, 
            CERT_SHA1_HASH_PROP_ID, hash, &hashSize)) {
            Logger::critical("NetworkClient::checkServerCert()> Error"
                " getting certificate hash.\n");
            CertFreeCertificateContext(pServerCert);
            WinHttpCloseHandle(hRequest_);
            hRequest_ = NULL;
            return false;
        }

        BYTE pfxHash[20]; // SHA-1 hash size
        DWORD pfxHashSize = sizeof(pfxHash);
        if (!CertGetCertificateContextProperty(pCertContext_, 
            CERT_SHA1_HASH_PROP_ID, pfxHash, &pfxHashSize)) {
            Logger::critical("NetworkClient::checkServerCert()>"
                " Error getting PFX certificate hash.\n");
            CertFreeCertificateContext(pServerCert);
            WinHttpCloseHandle(hRequest_);
            hRequest_ = NULL;
            return false;
        }

        bool isMatch = (hashSize == pfxHashSize) 
            && (memcmp(hash, pfxHash, hashSize) == 0);
        CertFreeCertificateContext(pServerCert);
        WinHttpCloseHandle(hRequest_);
        hRequest_ = NULL;
        return isMatch;
    }


    NetworkClient::RESULT_TYPE NetworkClient::get(const std::wstring& path, 
        char* buf, uint32_t length) {
        if (!buf) {
            Logger::fatal("NetworkClient::get()> Null buffer provided.\n");
            return ERROR_INVALID_PARAMETER; // Ensure this constant is defined
                                            // appropriately
        }

        HINTERNET hSession = nullptr, hConnect = nullptr, hRequest = nullptr;
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        BOOL bResults = FALSE;
        RESULT_TYPE result = RESULT_SUCCESS;

        // Open an HTTP session
        hSession = WinHttpOpen(SharedConstants::USER_AGENT, 
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, 
            WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) {
            Logger::recoverable_error("NetworkClient::get()> Error in"
                " WinHttpOpen: %u.\n", GetLastError());
            return GetLastError();
        }

        // Connect to the HTTP server
        hConnect = WinHttpConnect(hSession, url_.c_str(), port_, 0);
        if (!hConnect) {
            Logger::recoverable_error("NetworkClient::get()> Error in"
                " WinHttpConnect: %u.\n", GetLastError());
            WinHttpCloseHandle(hSession);
            return GetLastError();
        }

        // Create an HTTP request
        DWORD flags = use_ssl_ ? WINHTTP_FLAG_SECURE : 0;
        hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), 
            NULL, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            Logger::recoverable_error("NetworkClient::get()> Error in"
                " WinHttpOpenRequest: %u.\n", GetLastError());
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return GetLastError();
        }

        // Send the request
        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 
            0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            Logger::recoverable_error("NetworkClient::get()> Error in"
                " WinHttpSendRequest: %u.\n", GetLastError());
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return GetLastError();
        }

        // Wait for the response
        if (!WinHttpReceiveResponse(hRequest, NULL)) {
            Logger::recoverable_error("NetworkClient::get()> Error in"
                " WinHttpReceiveResponse: %u.\n", GetLastError());
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return GetLastError();
        }

        // Process the response
        do {
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                Logger::recoverable_error("NetworkClient::get()> Error"
                    " in WinHttpQueryDataAvailable: %u.\n", GetLastError());
                result = GetLastError();
                break;
            }

            if (dwSize == 0) break;  // No more data available

            dwSize = dwSize < length ? dwSize : length;
            ZeroMemory(buf, length);
            if (!WinHttpReadData(hRequest, (LPVOID)buf, dwSize, 
                &dwDownloaded)) {
                Logger::recoverable_error("NetworkClient::get()> Error"
                    " in WinHttpReadData: %u.\n", GetLastError());
                result = GetLastError();
                break;
            }

            if (dwDownloaded != dwSize) {
                Logger::recoverable_error("NetworkClient::get()> Partial"
                    " data read; expected more data.\n");
                result = ERROR_HANDLE_EOF; 
                break;
            }
        } while (dwSize > 0);

        // Close all handles
        if (hRequest) WinHttpCloseHandle(hRequest);
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (hSession) WinHttpCloseHandle(hSession);

        return result;
    }


    string NetworkClient::getLogzillaVersion() {
        char recv_buffer[100];
        RESULT_TYPE result = get(config_->version_path_.c_str(), recv_buffer, sizeof(recv_buffer));
        if (result != RESULT_SUCCESS) {
            Logger::recoverable_error("NetworkClient::getLogzillaVersion()>"
                " Error %u in get.\n", result);
            return "";
        }
        return string(recv_buffer);

    }

}


# if LOAD_PFX_WITH_PASSWORD
HCERTSTORE hPFXCertStore = NULL;
PCCERT_CONTEXT pPFXCertContext = NULL;

// Load the PFX file
HANDLE hFile = CreateFile(L"path_to_your.pfx", GENERIC_READ, 
    FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
DWORD fileSize = GetFileSize(hFile, NULL);
BYTE* pfxBuffer = new BYTE[fileSize];

DWORD bytesRead = 0;
ReadFile(hFile, pfxBuffer, fileSize, &bytesRead, NULL);
CloseHandle(hFile);

CRYPT_DATA_BLOB pfxBlob;
pfxBlob.pbData = pfxBuffer;
pfxBlob.cbData = fileSize;

// Use the password for the PFX file here
LPCWSTR szPassword = L"your_pfx_password";

hPFXCertStore = PFXImportCertStore(&pfxBlob, szPassword, 0);
if (!hPFXCertStore) {
    // Handle error
}

delete[] pfxBuffer; 
#endif

