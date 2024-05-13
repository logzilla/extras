/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once
#include "winhttp.h"
#include "windows.h"
#include <string>
#include "Configuration.h"

namespace Syslog_agent {

    class NetworkClient
    {
    public:
        static constexpr int MESSAGE_BUFFER_SIZE = 64 * 1024 * 1024;
        typedef DWORD RESULT_TYPE;
        static constexpr RESULT_TYPE RESULT_SUCCESS = ERROR_SUCCESS;
        NetworkClient::NetworkClient()
            : use_ssl_(false),
            url_(L""),
            hSession_(NULL),
            hConnect_(NULL),
            hRequest_(NULL),
            pCertContext_(NULL),
            hCertStore_(NULL),
            pfxBuffer_(NULL),
            config_(NULL),
            port_(0),
            server_cert_checked_(false)
        { }
        ~NetworkClient();

        bool initialize(const Configuration* config, const wstring api_key,
            const std::wstring& url, bool use_ssl, unsigned int port = 0);
        bool initialize(const Configuration* config, const wstring api_key, 
            const std::wstring& url);
        bool loadCertificate(const std::wstring& cert_path);
        bool connect();
        RESULT_TYPE post(const char* buf, uint32_t length);
        RESULT_TYPE post(const std::wstring& data);
        RESULT_TYPE post(const std::string & data);
        RESULT_TYPE get(const std::wstring& url, char* buf, uint32_t length);
        bool checkServerCert();
        bool readResponse(std::string& response) const;
        void close();
        string getLogzillaVersion();

    private:
        wstring api_key_;
        const Configuration *config_;
        bool use_ssl_;
        std::wstring url_;
        int port_;
        HINTERNET hSession_;
        HINTERNET hConnect_;
        HINTERNET hRequest_;
        PCCERT_CONTEXT pCertContext_;
        HCERTSTORE hCertStore_;
        BYTE* pfxBuffer_;
        bool server_cert_checked_;
    };

}