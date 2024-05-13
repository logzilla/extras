#pragma once
#include "stdafx.h"
#include <memory>
#include <thread>
#include <vector>
#include "NetworkClient.h"
#include "WindowsEvent.h"

using namespace std;

class PersistentConnections
{
public:
	PersistentConnections(vector<shared_ptr<NetworkClient>>& network_clients);
	~PersistentConnections();
	bool start(int msec_between_retries);
	bool stop();
	void waitForEnd();

private:
	volatile bool stop_requested_;
	vector<shared_ptr<NetworkClient>> network_clients_;
	unique_ptr<thread> connection_thread_;
	int msec_between_retries_;
	WindowsEvent stop_event_{ L"LogZilla_SyslogAgent_PersistentConnections" };

	void connectThread();

	friend void connectThreadStart(PersistentConnections* pers_connections);
};

