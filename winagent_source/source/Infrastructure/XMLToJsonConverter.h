#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>

class XMLToJSONConverter {
public:
    static std::string convert(const std::string& xml);
    static std::string convert(const char* utf8Xml);  // zero-terminated UTF-8 string
    static std::string convert(const char* utf8Xml, size_t bufferSize);  // UTF-8 string with explicit size
    static std::string escapeJSONString(const std::string& input);

private:
    struct XMLNode {
        std::string name;
        std::string text;
        std::map<std::string, std::string> attributes;
        std::vector<XMLNode> children;
    };

    static XMLNode parseXML(const std::string& xml, size_t& pos);
    static std::string nodeToJSON(const XMLNode& node);

    XMLToJSONConverter() = delete;
    ~XMLToJSONConverter() = delete;
    XMLToJSONConverter(const XMLToJSONConverter&) = delete;
    XMLToJSONConverter& operator=(const XMLToJSONConverter&) = delete;
};
