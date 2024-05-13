/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#include "stdafx.h"

// disabled
//#if 0 // disabled
//
//#include <ctime>
//#include "Syslog_server.h"
//#include "Logger.h"
//
//#define PING_INTERVAL 20.0
//#define TCP_IDLE_INTERVAL 60.0
//
//using namespace Syslog_agent;
//
//Syslog_server::Syslog_server(Network_connection* connection, int server_types) {
//    this->connection = connection;
//    this->server_types = server_types;
//    is_open = false;
//    sent = false;
//    open_fails = 0;
//    wait_to_retry = 0;
//    last_successful_ping = 0;
//    last_successful_send = 0;
//}
//
//void Syslog_server::open(const wchar_t* host, const wchar_t* port) {
//    if (ready_to_ping()) {
//        Logger::debug("ping");
//        if (check_bad_connection(connection->ping(host))) {
//            return;
//        }
//        last_successful_ping = connection->now();
//    }
//    if (is_open) return;
//    if (open_fails > 0 && !(server_types & primary)) return;
//    if (wait_to_retry > 0) {
//        Logger::debugf("open connection retry sleep %d", wait_to_retry);
//        connection->wait();
//        wait_to_retry--;
//        return;
//    }
//    if (check_bad_connection(connection->open(host, port, server_types & tcp ? IPPROTO_TCP : IPPROTO_UDP))) {
//        return;
//    }
//    is_open = true;
//    open_fails = 0;
//}
//
//bool Syslog_server::ready_to_ping() const {
//    if (wait_to_retry > 0) return false;
//    if (!(server_types & pings)) return false;
//    if (!is_open) return true;
//    return difftime(connection->now(), last_successful_ping) >= PING_INTERVAL;
//}
//
//bool Syslog_server::check_bad_connection(Result result) {
//    if (result.is_good()) return false;
//    result.log();
//    open_fails++;
//    wait_to_retry =
//        open_fails < 4 ? 2 :
//        open_fails < 11 ? 10 :
//        60;
//    close();
//    return true;
//}
//
//void Syslog_server::send(string& data) {
//    sent = false;
//    if (!is_open) return;
//    auto message = server_types & tcp ? (to_string(data.size()) + " " + data) : data;
//    auto result = connection->send(message.c_str(), (int)message.size());
//    if (!result.is_good()) {
//        result.log();
//        close();
//        return;
//    }
//    sent = true;
//    last_successful_send = connection->now();
//}
//
//bool Syslog_server::has_sent() const { return sent; }
//
//void Syslog_server::idle() {
//    Logger::debug("connection idle");
//    open_fails = 0;
//    wait_to_retry = 0;
//    if (server_types & tcp && difftime(connection->now(), last_successful_send) >= TCP_IDLE_INTERVAL) {
//        close();
//    }
//}
//
//void Syslog_server::close() {
//    connection->close();
//    is_open = false;
//}
//
//#endif