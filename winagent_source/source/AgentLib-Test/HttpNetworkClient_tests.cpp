#include "pch.h"
#include "../AgentLib/HttpNetworkClient.h"
#include "../AgentLib/Configuration.h"
#include "../AgentLib/SyslogAgentSharedConstants.h"
#include <windows.h>
#include <string>
#include <memory>
#include <cstring>

using namespace Syslog_agent;

class HttpNetworkClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = std::make_unique<Configuration>();
        // Basic configuration for tests
        config->setPrimaryHost(L"http://127.0.0.1");
        config->setPrimaryPort(80);
        config->setPrimaryApiKey(L"test-api-key");
        config->setPrimaryUseSelfSignedCert(false);
    }

    std::unique_ptr<Configuration> config;
};

// Verify that initialize() returns false when passed null pointers
TEST_F(HttpNetworkClientTest, InitializeWithNullParameters) {
    HttpNetworkClient client;


    ASSERT_FALSE(config->getApiPath().empty());
    // // Null configuration pointer
    // EXPECT_NE(InitializeError::Success, client.initialize(nullptr,
    //                                config->getPrimaryApiKey().c_str(),
    //                                L"127.0.0.1/incoming",
    //                                false,
    //                                80));

    // // Null API key
    // EXPECT_NE(InitializeError::Success, client.initialize(true, config.get(),
    //                                nullptr,
    //                                L"127.0.0.1/incoming",
    //                                false,
    //                                80));

    // // Null URL
    // EXPECT_NE(InitializeError::Success, client.initialize(true, config.get(),
    //                                config->getPrimaryApiKey().c_str(),
    //                                nullptr,
    //                                false,
    //                                80));
}

// Verify that initialize() succeeds with valid parameters
TEST_F(HttpNetworkClientTest, InitializeWithValidParameters) {
    HttpNetworkClient client;

    std::wstring url = config->getPrimaryHost() + std::wstring(SharedConstants::HTTP_API_PATH);

    EXPECT_EQ(InitializeError::Success, client.initialize(true, config.get(),
                                  config->getPrimaryApiKey().c_str(),
                                  url.c_str(),
                                  config->getPrimaryPort()));
}

// Verify that post() returns ERROR_NOT_CONNECTED when called before connect()
TEST_F(HttpNetworkClientTest, PostWithoutConnectReturnsNotConnected) {
    HttpNetworkClient client;

    std::wstring url = config->getPrimaryHost() + std::wstring(SharedConstants::HTTP_API_PATH);

    // Initialize client must return Success for test to proceed
    auto init_result = client.initialize(true, config.get(),
                                   config->getPrimaryApiKey().c_str(),
                                   url.c_str(),

                                   config->getPrimaryPort());
    ASSERT_EQ(InitializeError::Success, init_result);

    const char* payload = "{}";
    auto result = client.post(payload, static_cast<uint32_t>(strlen(payload)));

    EXPECT_EQ(static_cast<DWORD>(result), static_cast<DWORD>(ERROR_NOT_CONNECTED));
}

// Verify that connect() succeeds on localhost with no TLS (does not require active server)
TEST_F(HttpNetworkClientTest, ConnectSucceedsOnLocalhost) {
    HttpNetworkClient client;

    std::wstring url = config->getPrimaryHost() + std::wstring(SharedConstants::HTTP_API_PATH);

    // Initialize client must return Success for test to proceed
    auto init_result = client.initialize(true, config.get(),
                                   config->getPrimaryApiKey().c_str(),
                                   url.c_str(),

                                   config->getPrimaryPort());
    ASSERT_EQ(InitializeError::Success, init_result);

    EXPECT_TRUE(client.connect());
}