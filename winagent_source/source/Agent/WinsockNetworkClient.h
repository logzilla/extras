#pragma once
#include "NetworkClient.h"
#include <mutex>
#include <string>
#include <thread>
#include <Windows.h>
#include <WS2tcpip.h>
#include "WindowsEvent.h"

using namespace std;

class WinsockNetworkClient : public NetworkClient
{
public:
	enum class ConnectionProtocolEnum { TCP, UDP };
	WinsockNetworkClient(
		ConnectionProtocolEnum connection_protocol,
		wstring remote_host_address,
		unsigned int remote_port
	) :
		connection_protocol_(connection_protocol),
		remote_host_address_(remote_host_address),
		remote_port_(remote_port),
		is_connected_(false),
		can_ping_(false),
		socket_(INVALID_SOCKET),
		ping_connection_(INVALID_SOCKET),
		ping_thread_(nullptr),
		ping_addrinfo_(nullptr),
		ping_timeout_msec_(0),
		ping_pause_msec_(0),
		stop_ping_requested_(false)
	{}

	int connect() override;
	int send(const char* msg_buffer, const int msg_len) override;
	int send(const char* msg_buffer, const int msg_len, int& error_code) override;
	void close() override;
	bool isConnected() override;
	wstring connectionName() override;
	string connectionNameUtf8() override;
	SOCKET getSocket() override { return socket_; }

private:

	typedef struct SendPingPacketStruct {
		BYTE type;          // ICMP packet type
		BYTE code;          // Type sub code
		USHORT checksum;
		USHORT id;
		USHORT seq;
		ULONG timestamp;    // not part of ICMP, but we need it
		ULONG contents[7];
	} SendPingPacket;

	typedef struct PingReplyStruct {
		BYTE h_len : 4;           // Length of the header in dwords
		BYTE version : 4;         // Version of IP
		ULONG contents[256];
	} PingReply;


	ConnectionProtocolEnum connection_protocol_;
	wstring remote_host_address_;
	unsigned int remote_port_;
	volatile bool is_connected_;
	volatile bool can_ping_;
	SOCKET socket_, ping_connection_;
	recursive_mutex connecting_;
	unique_ptr<thread> ping_thread_;
	PADDRINFOW ping_addrinfo_;
	int ping_timeout_msec_, ping_pause_msec_;
	volatile bool stop_ping_requested_;

	bool ping();
	bool openPing(DWORD timeout_msec);
	void formatPingRequest(SendPingPacket& pingPacket);
	void pingThread();

	friend void pingThreadExec(WinsockNetworkClient* client);

};
