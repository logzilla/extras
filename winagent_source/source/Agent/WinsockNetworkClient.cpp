#include "stdafx.h"
#include <clocale>
#include <codecvt>
#include <locale>
#include <sstream>
#include "Logger.h"
#include "WinsockNetworkClient.h"


#define ICMP_ECHO_REPLY 0
#define ICMP_ECHO_REQUEST 8
#define ICMP_MIN 8

int WinsockNetworkClient::connect() {

	if (is_connected_) {
		Logger::warn("WinsockNetworkClient::connect() already connected\n");
		return 0;
	}

	struct addrinfoW hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;

	switch (connection_protocol_) {
	case ConnectionProtocolEnum::TCP:
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		break;
	case ConnectionProtocolEnum::UDP:
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
		break;
	default:
		Logger::recoverable_error("WinsockNetworkClient: invalid protocol\n");
		return -1;
	}
	ADDRINFOW addrinfo_result;
	PADDRINFOW presult = &addrinfo_result;
	wchar_t portbuf[6];
	swprintf_s(portbuf, 6, L"%d", remote_port_);
	int result_code;
	result_code = GetAddrInfoW(remote_host_address_.c_str(), portbuf, &hints, &presult);
	if (result_code != 0) {
		Logger::fatal("WinsockNetworkClient::connect(): getaddrinfo failed with error: %d\n", result_code);
		return -1;
	}

	socket_ = WSASocket(AF_INET, hints.ai_socktype, hints.ai_protocol, NULL, 0, 0);
	if (socket_ == INVALID_SOCKET) {
		Logger::fatal("WinsockNetworkClient::connect() socket failed with error: %ld\n", WSAGetLastError());
		return -1;
	}

#ifndef SOL_TCP
#define SOL_TCP 6  // socket options TCP level
#endif
#ifndef TCP_USER_TIMEOUT
#define TCP_USER_TIMEOUT 18  // how long for loss retry before timeout [ms]
#endif
	int timeout = 8000;
	if (setsockopt(socket_, SOL_TCP, TCP_USER_TIMEOUT, (char*)&timeout, sizeof(timeout))) {
		Logger::debug("WinsockNetworkClient::connect() could not set connection timeout: %ld\n", WSAGetLastError());
	}

	int connection_result = 0;
	if (is_connected_) {
		if (socket_ && socket_ != INVALID_SOCKET) {
			close();
		}
		return 0;
	}

	DWORD msec_timeout = 5000;
	if (setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, (char*)&msec_timeout, sizeof(msec_timeout)) < 0) {
		Logger::critical("WinsockNetworkClient::connect() setsockopt send timeout failed with error: %ld\n", WSAGetLastError());
		if (socket_ && socket_ != INVALID_SOCKET) {
			close();
		}
		return -1;
	}
	
	connecting_.lock();
	//Logger::debug2("WinsockNetworkClient::connect() trying connection to remote %s\n", connectionNameUtf8().c_str());
	if (is_connected_) {
		connecting_.unlock();
		return 0;
	}

	if (tls_config_) {
		tls_config_->setServerCertPem(server_cert_pem_.get());
		tls_config_->setupTlsForConnection();
	}

	result_code = ::connect(socket_, presult->ai_addr, (int)presult->ai_addrlen);
	if (result_code) {
		connection_result = WSAGetLastError();
		close();
		Logger::debug("WinsockNetworkClient::connect() connection to remote %s failed (%d)\n", connectionNameUtf8().c_str(), connection_result);
	}
	else {
		if (tls_config_) {
			is_connected_ = tls_config_->doHandshake(socket_);
			if (is_connected_) {
				Logger::debug("WinsockNetworkClient::connect() TLS connection to remote %s success\n", connectionNameUtf8().c_str());
				connection_result = 0;
			}
			else {
				Logger::debug("WinsockNetworkClient::connect() TLS connection to remote %s failed\n", connectionNameUtf8().c_str());
				connection_result = SOCKET_ERROR;
			}
		}
		else {
			is_connected_ = true;
			Logger::debug("WinsockNetworkClient::connect() connection to remote %s success\n", connectionNameUtf8().c_str());
			connection_result = 0;
		}
	}

	connecting_.unlock();

	return connection_result;
}

int WinsockNetworkClient::send(const char* msg_buffer, const int msg_len) {
	int error_code;
	return send(msg_buffer, msg_len, error_code);
}

int WinsockNetworkClient::send(const char* msg_buffer, const int msg_len, int& error_code) {

	fd_set socket_set;
	FD_ZERO(&socket_set);
	FD_SET(socket_, &socket_set);
	TIMEVAL timeout;
	timeout.tv_sec = 8;
	timeout.tv_usec = 0;
	int select_result = select(0, NULL, &socket_set, NULL, &timeout);
	if (select_result != 1) {
		Logger::recoverable_error("WinsockNetworkClient::send() select returned %d\n", select_result);
		return -1;
	}
	int number_bytes_sent = 0;
	if (tls_config_) {
		number_bytes_sent = tls_config_->tls_send(msg_buffer, msg_len);
	}
	else {
		number_bytes_sent = ::send(socket_, msg_buffer, msg_len, 0);
	}
	if (number_bytes_sent < 1) {
		error_code = WSAGetLastError();
		Logger::warn("WinsockNetworkClient::send() sent 0 bytes, error %d\n", error_code);
	}
	else {
		error_code = 0;
	}
	return number_bytes_sent;
}


wstring WinsockNetworkClient::connectionName() {
	wchar_t* protocol_name;
	wchar_t buf[1024];

	switch (connection_protocol_) {
	case ConnectionProtocolEnum::TCP:
		protocol_name = L"TCP";
		break;
	case ConnectionProtocolEnum::UDP:
		protocol_name = L"UDP";
		break;
	default:
		protocol_name = L"UNK";
		break;
	}
	swprintf_s(buf, 1024, L"%s:%s:%d", protocol_name, remote_host_address_.c_str(), remote_port_);
	return wstring(buf);
}


void WinsockNetworkClient::close() {
	if (socket_ && socket_ != INVALID_SOCKET) {
		connecting_.lock();
		if (is_connected_) {
			Logger::debug("WinsockNetworkClient::close() closing connection\n");
		}
		if (tls_config_) {
			tls_config_->close();
			tls_config_->cleanup();
		}
		is_connected_ = false;
		shutdown(socket_, SD_BOTH);
		closesocket(socket_);
		socket_ = INVALID_SOCKET;
		connecting_.unlock();
	}

}

std::string ws2s(const std::wstring& wstr)
{
	using convert_typeX = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> converterX;

	return converterX.to_bytes(wstr);
}

string WinsockNetworkClient::connectionNameUtf8() {
	wstring wstr = connectionName();
	return ws2s(wstr);
}

bool WinsockNetworkClient::isConnected() {
	if (connection_protocol_ == ConnectionProtocolEnum::UDP
		&& ping_thread_
		&& !can_ping_)
	{
		return false;
	}
	return is_connected_;
}
