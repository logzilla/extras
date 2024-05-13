/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#include "stdafx.h"
#include <assert.h>
#include "gnutls\gnutls.h"

#include "Logger.h"
#include "NetworkClient.h"
#include "TLS.h"

#pragma comment(lib, "libgnutls-30.lib")

#define MAX_BUF 1024
#define MSG "GET / HTTP/1.0\r\n\r\n"

#if DEBUG
#define CHECK(x) assert((x)>=0)
#define LOOP_CHECK(rval, cmd) \
        do { \
                rval = cmd; \
        } while(rval == GNUTLS_E_AGAIN || rval == GNUTLS_E_INTERRUPTED); \
        assert(rval >= 0)
#else
#define CHECK(x) { (x); }
#define LOOP_CHECK(rval, cmd) \
        do { \
                rval = cmd; \
        } while(rval == GNUTLS_E_AGAIN || rval == GNUTLS_E_INTERRUPTED);
#endif


uint8_t* TLS::cert_der_data_s_ = nullptr;
uint32_t TLS::cert_der_length_s_ = 0;


uint8_t TLS::base64Decode(uint8_t ch)
{
    uint8_t ret = 0;
    if (ch >= 'A' && ch <= 'Z')
    {
        ret = ch - 'A';
    }
    else if (ch >= 'a' && ch <= 'z')
    {
        ret = ch - 'a' + 26;
    }
    else if (ch >= '0' && ch <= '9')
    {
        ret = ch - '0' + 52;
    }
    else if (ch == '+')
    {
        ret = 62;
    }
    else if (ch == '/')
    {
        ret = 63;
    }

    return (ret);
}


int TLS::convertPemCertToDer(const uint8_t* pem, uint32_t plen, uint8_t** der,
    uint32_t* dlen)
{
    int value = 0;
    uint32_t i;
    uint32_t j;
    int padZero = 0;
    uint8_t* derPtr;
    uint32_t derLen;

    if ((pem == NULL) || (der == NULL))
    {
        return (-1);
    }

    for (i = plen; pem[i - 1] == '='; i--)
    {
        padZero++;
    }

    /* Base64 decode: 4 characters to 3 bytes */
    derLen = (plen / 4) * 3;
    derPtr = (uint8_t*)malloc(derLen);
    if (!derPtr)
    {
        return (-1);
    }

    for (i = 0, j = 0; (i + 3) < plen && (j + 2) < derLen; i += 4, j += 3)
    {
        value = (base64Decode(pem[i]) << 18) + (base64Decode(pem[i + 1]) << 12)
            + (base64Decode(pem[i + 2]) << 6) + base64Decode(pem[i + 3]);

        derPtr[j] = (value >> 16) & 0xFF;
        derPtr[j + 1] = (value >> 8) & 0xFF;
        derPtr[j + 2] = value & 0xFF;
    }

    /* Actual length of buffer filled */
    *dlen = derLen - padZero;
    *der = derPtr;

    return (0);
}


int verifyCertificate(gnutls_session_t session) {

    unsigned int cert_list_size;
    const gnutls_datum_t* raw_cert = gnutls_certificate_get_peers(session, &cert_list_size);
    Logger::debug2("verifyCertificate() verifying cert: list_size=%d, cert size=%d\n", 
        cert_list_size, raw_cert->size);
    if (TLS::getCertDerLength() != raw_cert->size) {
        return -1;
    }
    uint8_t* cert_der = TLS::getCertDerData();
    for (uint32_t i = 0; i < raw_cert->size; ++i) {
        if (raw_cert->data[i] != cert_der[i]) {
            return -1;
        }
    }
    return 0;
}


void TLS::setServerCertPem(const char * const server_cert_pem) {
    if (cert_der_data_s_) {
        free(cert_der_data_s_);
    }

    auto server_cert_buf = make_unique<char[]>(strlen(server_cert_pem));
    char* buf_ptr = server_cert_buf.get();
    char* pem_ptr = const_cast<char*>(server_cert_pem);
    const char* PEM_HEADER = "-----BEGIN CERTIFICATE-----";
    const int PEM_HEADER_LEN = (const int) strlen(PEM_HEADER);
    const char* PEM_FOOTER = "-----END CERTIFICATE-----";
    const int PEM_FOOTER_LEN = (const int) strlen(PEM_FOOTER);
    if (strncmp(pem_ptr, PEM_HEADER, PEM_HEADER_LEN) == 0) {
        pem_ptr += PEM_HEADER_LEN;
    }
    int buf_len = 0;
    for (; *pem_ptr != 0; ++pem_ptr) {
        if (strncmp(pem_ptr, PEM_FOOTER, PEM_FOOTER_LEN) == 0) {
            break;
        }
        else if (*pem_ptr != 13 && *pem_ptr != 10) {
            *(buf_ptr + buf_len++) = *pem_ptr;
        }
    }
    *(buf_ptr + buf_len) = 0;

    const uint8_t* pem = reinterpret_cast<const uint8_t*>(buf_ptr);
    convertPemCertToDer(pem, buf_len, &cert_der_data_s_, &cert_der_length_s_);
}


void TLS::setupTlsForConnection() {

    if (gnutls_check_version("3.6.6") == NULL) {
        Logger::fatal("TLS::setupTlsForConnection() GnuTLS 3.4.6 or later"
            " is required\n");
    }

    /* for backwards compatibility with gnutls < 3.3.0 */
    CHECK(gnutls_global_init());

    /* X509 stuff */
    CHECK(gnutls_certificate_allocate_credentials(&xcred_));

    gnutls_certificate_set_verify_function(xcred_, verifyCertificate);

    /* Initialize TLS session */
    CHECK(gnutls_init(&session_, GNUTLS_CLIENT));

    /* It is recommended to use the default priorities */
    CHECK(gnutls_set_default_priority(session_));

    /* put the x509 credentials to the current session */
    CHECK(gnutls_credentials_set(session_, GNUTLS_CRD_CERTIFICATE, xcred_));

    clean_ = false;
}


bool TLS::doHandshake(SOCKET socket) {

    int ret;
    char* desc;
    gnutls_datum_t out;
    gnutls_certificate_type_t type;
    unsigned status;

    socket_ = socket;

    gnutls_transport_set_int(session_, static_cast<int>(socket_));
    // gnutls_handshake_set_timeout(session_, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);
    gnutls_handshake_set_timeout(session_, 5000 /* msec */);

    /* Perform the TLS handshake */
    do {
        ret = gnutls_handshake(session_);
    } while (ret < 0 && gnutls_error_is_fatal(ret) == 0);
    if (ret < 0) {
        if (ret == GNUTLS_E_CERTIFICATE_VERIFICATION_ERROR) {
            /* check certificate verification status */
            type = gnutls_certificate_type_get(session_);
            status = gnutls_session_get_verify_cert_status(session_);
            CHECK(gnutls_certificate_verification_status_print(status,
                type, &out, 0));
            Logger::fatal("TLS::setupTlsForConnection() error cert verify"
                " output: %s\n", out.data);
            // gnutls_free(out.data);
        }
        Logger::fatal("TLS::setupTlsForConnection() *** Handshake failed:"
            " %s\n", gnutls_strerror(ret));
        return false;
    }
    else {
        desc = gnutls_session_get_desc(session_);
        Logger::debug("TLS::testForHostAddress() TLS succeeded, session"
            " info: %s\n", desc);
        gnutls_free(desc);
        return true;
    }
}


void TLS::close() {
    CHECK(gnutls_bye(session_, GNUTLS_SHUT_RDWR));
}


void TLS::cleanup() {
    if (!clean_) {
        gnutls_deinit(session_);
        gnutls_certificate_free_credentials(xcred_);
        gnutls_global_deinit();
        clean_ = true;
    }
}


int TLS::tls_send(const char* message, size_t message_length) const {
    return (int) gnutls_record_send(session_, static_cast<const void*>(message), message_length);
}


int TLS::tls_receive(char* buffer, size_t buffer_size) const {
    return (int) gnutls_record_recv(session_, buffer, buffer_size);
}
