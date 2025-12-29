/*
Copyright 2021 Logzilla Corp.
*/

#pragma once

#include <cstring>
#include <cstdio>

class EventXmlData {
public:
    // sizing constants - tweak if you know you need more room
    static constexpr int MAX_PROVIDER_NAME = 128;
    static constexpr int MAX_GUID = 64;
    static constexpr int MAX_SMALL = 32;
    static constexpr int MAX_MEDIUM = 64;
    static constexpr int MAX_LARGE = 256;
    static constexpr int MAX_KEYWORDS = 32;
    static constexpr int MAX_TIME = 32;
    static constexpr int MAX_DATA_NAME = 64;
    static constexpr int MAX_DATA_VALUE = 4096;
    static constexpr int MAX_DATAS = 24;

    // top-level fields
    char providerName[MAX_PROVIDER_NAME];
    char providerGuid[MAX_GUID];
    char eventID[MAX_SMALL];
    char qualifiers[MAX_SMALL];
    char version[MAX_SMALL];
    char level[MAX_SMALL];
    char task[MAX_SMALL];
    char opcode[MAX_SMALL];
    char keywords[MAX_KEYWORDS];
    char systemTime[MAX_TIME];
    char eventRecordID[MAX_SMALL];
    char activityID[MAX_GUID];
    char processID[MAX_SMALL];
    char threadID[MAX_SMALL];
    char channel[MAX_MEDIUM];
    char computer[MAX_MEDIUM];
    char userID[MAX_SMALL];

    // repeated <Data> elements
    struct Data {
        char name[MAX_DATA_NAME];
        char value[MAX_DATA_VALUE];
    } data[MAX_DATAS];
    int dataCount = 0;

    /// Parses the XML in-place (read-only) and fills all buffers.
    /// Returns true if at least the Provider tag was found.
    bool parse(const char* xml) {
        dataCount = 0;
        bool ok = false;

        ok |= extractAttribute(xml, "<Provider", "Name", providerName, MAX_PROVIDER_NAME);
        extractAttribute(xml, "<Provider", "Guid", providerGuid, MAX_GUID);

        ok |= extractAttribute(xml, "<EventID", "Qualifiers", qualifiers, MAX_SMALL);
        ok |= extractElement(xml, "EventID", eventID, MAX_SMALL);
        ok |= extractElement(xml, "Version", version, MAX_SMALL);
        ok |= extractElement(xml, "Level", level, MAX_SMALL);
        ok |= extractElement(xml, "Task", task, MAX_SMALL);
        ok |= extractElement(xml, "Opcode", opcode, MAX_SMALL);
        ok |= extractElement(xml, "Keywords", keywords, MAX_KEYWORDS);
        ok |= extractAttribute(xml, "<TimeCreated", "SystemTime", systemTime, MAX_TIME);
        ok |= extractElement(xml, "EventRecordID", eventRecordID, MAX_SMALL);
        extractAttribute(xml, "<Correlation", "ActivityID", activityID, MAX_GUID);
        extractAttribute(xml, "<Execution", "ProcessID", processID, MAX_SMALL);
        extractAttribute(xml, "<Execution", "ThreadID", threadID, MAX_SMALL);
        ok |= extractElement(xml, "Channel", channel, MAX_MEDIUM);
        ok |= extractElement(xml, "Computer", computer, MAX_MEDIUM);
        extractAttribute(xml, "<Security", "UserID", userID, MAX_SMALL);

        // collect all <Data>.</Data>
        const char* p = xml;
        while (dataCount < MAX_DATAS
            && (p = std::strstr(p, "<Data")) != nullptr)
        {
            // name attribute (optional)
            extractAttribute(p, "<Data", "Name", data[dataCount].name, MAX_DATA_NAME);

            // inner text
            const char* start = std::strchr(p, '>');
            if (!start) break;
            ++start;
            const char* end = std::strstr(start, "</Data>");
            if (!end) break;
            size_t len = end - start;
            if (len >= MAX_DATA_VALUE) len = MAX_DATA_VALUE - 1;
            strncpy_s(data[dataCount].value, MAX_DATA_VALUE, start, len);

            ++dataCount;
            p = end + 7;  // move past "</Data>"
        }

        return ok;
    }

private:
    // find tag, then attr="."
    static bool extractAttribute(const char* xml,
        const char* tag,
        const char* attr,
        char* out, int maxLen)
    {
        const char* p = std::strstr(xml, tag);
        if (!p) { out[0] = '\0'; return false; }
        const char* a = std::strstr(p, attr);
        if (!a) { out[0] = '\0'; return false; }
        a = std::strchr(a, '=');
        if (!a || a[1] != '\'') { out[0] = '\0'; return false; }
        a += 2;  // skip ='
        const char* end = std::strchr(a, '\'');
        if (!end) { out[0] = '\0'; return false; }
        size_t len = end - a;
        if (len >= size_t(maxLen)) len = maxLen - 1;
        strncpy_s(out, maxLen, a, len);
        return true;
    }

    // find <Tag>.</Tag>
    static bool extractElement(const char* xml,
        const char* tag,
        char* out, int maxLen)
    {
        char openTag[32], closeTag[32];
        std::snprintf(openTag, sizeof(openTag), "<%s", tag);
        const char* p = std::strstr(xml, openTag);
        if (!p) { out[0] = '\0'; return false; }
        const char* gt = std::strchr(p, '>');
        if (!gt) { out[0] = '\0'; return false; }
        ++gt;
        std::snprintf(closeTag, sizeof(closeTag), "</%s>", tag);
        const char* end = std::strstr(gt, closeTag);
        if (!end) { out[0] = '\0'; return false; }
        size_t len = end - gt;
        if (len >= size_t(maxLen)) len = maxLen - 1;
        strncpy_s(out, maxLen, gt, len);
        return true;
    }
};
#pragma once
