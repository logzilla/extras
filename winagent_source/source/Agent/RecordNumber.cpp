/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#include "stdafx.h"
#include "RecordNumber.h"

using namespace Syslog_agent;

RecordNumber::RecordNumber(DWORD value) { this->value = value; }

RecordNumber::RecordNumber(RecordNumber& other) { value = other.value; }

void RecordNumber::increment() {
    value = value == ULONG_MAX ? 0 : value + 1;
}

bool RecordNumber::is_greater(RecordNumber& other) const {
    return
        (other.value > value&& other.value - value >= ULONG_MAX / 2) ||
        (other.value < value && value - other.value < ULONG_MAX / 2);
}

RecordNumber::operator DWORD () const { return value; }

DWORD RecordNumber::operator=(DWORD new_value) {
    value = new_value;
    return new_value;
}
