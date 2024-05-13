/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#include "stdafx.h"
#include "Options.h"

using namespace Syslog_agent;

Options::Options(int count, wchar_t** values) {
	this->count = count;
	this->values = values;
}

bool Options::has(wchar_t* option) const {
	for (auto i = 1; i < count; i++) {
		if (!_wcsicmp(values[i], option)) return true;
	}
	return false;
}

wchar_t* Options::getArgument(wchar_t* option) const {
	for (auto i = 1; i < count; i++) {
		if (!_wcsicmp(values[i], option)) {
			if (i < count - 1) {
				return values[i + 1];
			}
			else {
				return nullptr;
			}
		}
	}
	return nullptr;
}
