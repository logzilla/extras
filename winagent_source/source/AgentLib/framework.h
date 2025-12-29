#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX                        // Prevents Windows.h from defining min/max macros
// Windows Header Files
#include <windows.h>
// Project-wide definitions
#ifdef AGENTLIB_EXPORTS
#define AGENTLIB_API __declspec(dllexport)
#else
#define AGENTLIB_API __declspec(dllimport)
#endif

#define MAX_LOGGER_KEY_LENGTH 256

#ifdef SIMPLE_LOGTHIS
#define LOG_THIS [&]() { \
    char buf[MAX_LOGGER_KEY_LENGTH]; \
    const char* typeName = typeid(*this).name(); \
    const char* funcName = __func__; \
    _snprintf_s(buf, MAX_LOGGER_KEY_LENGTH, _TRUNCATE, "::%s::%s", typeName, funcName); \
    return Logger::getLoggerByKey(buf); \
}()
#else
#define LOG_THIS [&]() { \
    char buf[MAX_LOGGER_KEY_LENGTH]; \
    const char* prettyFunction = __FUNCSIG__; \
    size_t start = 0; \
    while (prettyFunction[start] && prettyFunction[start] != ' ') ++start; \
    while (prettyFunction[start] && prettyFunction[start] == ' ') ++start; \
    while (prettyFunction[start] && prettyFunction[start] != ' ') ++start; \
    while (prettyFunction[start] && prettyFunction[start] == ' ') ++start; \
    size_t end = start; \
    while (prettyFunction[end] && prettyFunction[end] != '(') ++end; \
    size_t len = end - start; \
    if (len >= MAX_LOGGER_KEY_LENGTH) len = MAX_LOGGER_KEY_LENGTH - 1; \
    memcpy(buf, prettyFunction + start, len); \
    buf[len] = '\0'; \
    return Logger::getLoggerByKey(buf); \
}()
#endif

#define LAST_RESORT_LOGGER Logger::getLoggerByKey(Logger::LAST_RESORT_LOGGER_NAME)
