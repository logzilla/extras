/*
SyslogAgent: a syslog agent for Windows
Copyright 2021 Logzilla Corp.
*/

#include "stdafx.h"
#include "Options.h"

using namespace Syslog_agent;

Options::Options(int count, const wchar_t* const* values) {
	this->count = count;
	this->values = values;
}

bool Options::has(const wchar_t* option) const {
	for (auto i = 1; i < count; i++) {
		if (!_wcsicmp(values[i], option)) return true;
	}
	return false;
}

const wchar_t* Options::getArgument(const wchar_t* option) const {
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
