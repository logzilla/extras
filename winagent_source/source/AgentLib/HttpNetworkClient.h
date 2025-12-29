#pragma once

#include <windows.h>
#include <winhttp.h>
#include <chrono>
#include "Configuration.h"
#include "INetworkClient.h"
#include "SyslogAgentSharedConstants.h"
#include "Globals.h"
#include "../Infrastructure/Logger.h"
#include "../Infrastructure/HandleGuard.h"
#include <string>

#ifndef ERROR_WINHTTP_CONNECTION_RESET
#define ERROR_WINHTTP_CONNECTION_RESET 12030L
#endif

#ifndef ERROR_WINHTTP_CONNECTION_ERROR
#define ERROR_WINHTTP_CONNECTION_ERROR 12030L
#endif

#ifndef ERROR_WINHTTP_OPERATION_CANCELLED
#define ERROR_WINHTTP_OPERATION_CANCELLED 12017L
#endif

namespace Syslog_agent {

    class AGENTLIB_API HttpNetworkClient : public INetworkClient
    {
    public:
        static constexpr int MESSAGE_BUFFER_SIZE = 64 * 1024 * 1024;
        static constexpr size_t MAX_URL_LENGTH = 2048;
        static constexpr size_t MAX_API_KEY_LENGTH = 256;
        static constexpr size_t MAX_HEADERS_LENGTH = 4096;
        static constexpr size_t MAX_PATH_LENGTH = 2048;
        static constexpr size_t MAX_RESPONSE_LENGTH = 4096;

        static constexpr std::chrono::seconds KEEP_ALIVE_TIMEOUT{ 60 };  // 60 second keep-alive
        static constexpr DWORD DEFAULT_CONNECT_TIMEOUT = 30000;   // 30 seconds
        static constexpr DWORD DEFAULT_SEND_TIMEOUT = 30000;      // 30 seconds
        static constexpr DWORD DEFAULT_RECEIVE_TIMEOUT = 30000;   // 30 seconds
        static constexpr DWORD MAX_REDIRECT_COUNT = 5;            // Maximum number of redirects to follow
        static constexpr DWORD MAX_DRAIN_TIME_MS = 5000;         // Max time for draining connection

        HttpNetworkClient();
        virtual ~HttpNetworkClient() override;

        // INetworkClient interface implementation
        virtual InitializeError initialize(bool is_primary, const Configuration* config, const wchar_t* api_key,
            const wchar_t* url, unsigned int port = 0) override;
        virtual bool connect() override;
        virtual RESULT_TYPE post(const char* buf, uint32_t length) override;
        virtual void close() override;
        virtual bool getLogzillaVersion(char* version_buf, size_t max_length, size_t& bytes_written) override;
        virtual SOCKET getSocket() override { return NULL; }  // HTTP doesn't use raw sockets

        // Additional HTTP-specific methods
        virtual bool loadCertificate(const wchar_t* cert_path);
        virtual bool get(const wchar_t* url, char* response_buffer, size_t max_length, size_t& bytes_written);

    protected:
        bool applyTimeouts(HINTERNET handle) const;
        bool negotiateCompression();
        void drainConnection();
        bool checkServerCert();
        bool followRedirect(wchar_t* redirect_buffer, size_t buffer_size);
        void cleanup_request();

        bool use_ssl_;
        bool use_compression_;
        bool use_self_signed_cert_;
        Infrastructure::WinHttpHandleGuard hSession_;
        Infrastructure::WinHttpHandleGuard hConnection_;
        Infrastructure::WinHttpHandleGuard hRequest_;
        DWORD connect_timeout_;
        DWORD send_timeout_;
        DWORD receive_timeout_;
        unsigned int port_;
        bool is_connected_;
        DWORD stored_security_flags_ = 0;
        std::recursive_mutex connecting_;

        // Fixed-size buffers instead of std::wstring
        wchar_t host_[MAX_URL_LENGTH];
        wchar_t path_[MAX_PATH_LENGTH];
        wchar_t api_key_[MAX_API_KEY_LENGTH];
        wchar_t url_[MAX_URL_LENGTH];

    };

} // namespace Syslog_agent
#pragma once
