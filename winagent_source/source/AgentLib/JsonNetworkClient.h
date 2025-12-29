#pragma once

#include <string>
#include <mutex>
#include <Windows.h>
#include "INetworkClient.h"
#include "framework.h"

namespace Syslog_agent {

    class Configuration;

    class AGENTLIB_API JsonNetworkClient : public INetworkClient {
    public:
        JsonNetworkClient(const std::wstring& hostname, unsigned int port);
        virtual ~JsonNetworkClient() override;

        // INetworkClient interface implementation
        virtual InitializeError initialize(bool is_primary, const Configuration* config, const wchar_t* api_key,
            const wchar_t* url, unsigned int port = 0) override {
            return InitializeError::Success;
        }  // No initialization needed for JSON client
        virtual bool connect() override;
        virtual RESULT_TYPE post(const char* buf, uint32_t length) override;
        virtual void close() override;
        virtual bool getLogzillaVersion(char* version_buf, size_t max_length, size_t& bytes_written) override;
        virtual SOCKET getSocket() override { return socket_; }

        std::wstring connectionName() { return remote_host_address_; }
        std::string connectionNameUtf8();

    private:
        std::wstring remote_host_address_;
        unsigned int remote_port_;
        bool is_connected_;
        SOCKET socket_;
        int connect_timeout_;
        int send_timeout_;
        int receive_timeout_;
        std::recursive_mutex connecting_;

        static const int DEFAULT_CONNECT_TIMEOUT = 10000;  // 10 seconds
        static const int DEFAULT_SEND_TIMEOUT = 30000;     // 30 seconds
        static const int DEFAULT_RECEIVE_TIMEOUT = 30000;  // 30 seconds
    };

} // namespace Syslog_agent
#pragma once
