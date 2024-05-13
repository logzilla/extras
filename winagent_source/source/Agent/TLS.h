/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once

#include <memory>
#if defined(_MSC_VER)
#include <WinSock2.h>
#else
#include <sys/socket.h>
#endif
#include "gnutls/gnutls.h"


/* Note:	The server cert verification is extremely simplistic but
			adequate for our purposes.  Given that the server cert 
			verification function must be static it is difficult 
			within the verification function to associate a specific
			cert to a specific connection.  
*/

using namespace std;

class NetworkClient;

class TLS {
public:
	static void setServerCertPem(const char * const server_cert_pem);
	static uint8_t* getCertDerData() { return cert_der_data_s_; }
	static uint32_t getCertDerLength() { return cert_der_length_s_; }

	TLS() : clean_(true), socket_(0), xcred_(0), session_(0) {}
	void setupTlsForConnection();
	bool doHandshake(SOCKET socket);
	int tls_send(const char* message, size_t message_length) const;
	int tls_receive(char* buffer, size_t buffer_size) const;
	void close();
	void cleanup();
	~TLS() { cleanup(); }

private:
	static uint8_t* cert_der_data_s_;
	static uint32_t cert_der_length_s_;

	bool clean_;
	SOCKET socket_;
	gnutls_certificate_credentials_t xcred_;
	gnutls_session_t session_;

	static uint8_t base64Decode(uint8_t ch);
	static int convertPemCertToDer(const uint8_t* pem, uint32_t plen, uint8_t** der, uint32_t* dlen);

};