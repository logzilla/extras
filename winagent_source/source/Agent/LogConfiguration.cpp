/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/


#include "stdafx.h"
#include "LogConfiguration.h"

using namespace Syslog_agent;

void LogConfiguration::loadFromRegistry(Registry& parent) {
    bookmark_ = Registry::readBookmark(channel_.c_str());
}

void LogConfiguration::saveToRegistry(Registry& parent) const {
    Registry::writeBookmark(channel_.c_str(), bookmark_.c_str());
}


