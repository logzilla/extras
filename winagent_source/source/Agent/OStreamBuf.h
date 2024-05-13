/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once
#include <streambuf>

template <typename char_type>
struct OStreamBuf : public std::basic_streambuf<char_type, std::char_traits<char_type> >
{
    OStreamBuf(char_type* buffer, std::streamsize bufferLength)
    {
        // set the "put" pointer the start of the buffer and record it's length.
        setp(buffer, buffer + bufferLength);
    }
};