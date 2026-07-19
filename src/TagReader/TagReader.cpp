/**
 * Author: James Brown 2018
 * Crawls JSON looking for tags.
 *
 * JSON handling is centralized here. The char* parser works in-place: it inserts
 * terminators into the caller-provided JSON buffer so keys and values point into
 * the original stack/heap data instead of allocating/copying each value pair.
 */

#include "TagReader.h"
#include <string.h>

using std::string;

TagReader::TagReader()
{
    _length = 0;
}

char *TagReader::skipWhitespace(char *p)
{
    while (p != nullptr && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) {
        ++p;
    }
    return p;
}

char *TagReader::parseString(char *p, char **out)
{
    if (p == nullptr || *p != '"') return nullptr;
    ++p;
    *out = p;

    bool escaped = false;
    while (*p != '\0') {
        if (escaped) {
            escaped = false;
        } else if (*p == '\\') {
            escaped = true;
        } else if (*p == '"') {
            *p = '\0';
            return p + 1;
        }
        ++p;
    }
    return nullptr;
}

char *TagReader::parsePrimitive(char *p, char **out)
{
    if (p == nullptr) return nullptr;
    p = skipWhitespace(p);
    *out = p;
    while (*p != '\0' && *p != ',' && *p != '}' && *p != ']' &&
           *p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') {
        ++p;
    }
    if (*p != '\0') {
        *p = '\0';
        return p + 1;
    }
    return p;
}

bool TagReader::equals(const char *a, const char *b)
{
    if (a == nullptr || b == nullptr) return false;
    return strcmp(a, b) == 0;
}

size_t TagReader::parse(char *jsonBuffer)
{
    _tags.clear();
    _length = 0;

    if (jsonBuffer == nullptr) return 0;

    char *p = jsonBuffer;
    while ((p = strchr(p, '"')) != nullptr) {
        char *key = nullptr;
        p = parseString(p, &key);
        if (p == nullptr) break;

        p = skipWhitespace(p);
        if (p == nullptr || *p != ':') {
            continue;
        }
        ++p;
        p = skipWhitespace(p);

        char *value = nullptr;
        if (p != nullptr && *p == '"') {
            p = parseString(p, &value);
        } else {
            p = parsePrimitive(p, &value);
        }
        if (p == nullptr || value == nullptr) break;

        Tag tag;
        tag.key = key;
        tag.value = value;
        _tags.push_back(tag);
    }

    return _tags.size();
}

const char *TagReader::getValue(const char *key) const
{
    for (size_t i = 0; i < _tags.size(); ++i) {
        if (equals(_tags[i].key, key)) {
            _length = strlen(_tags[i].value);
            return _tags[i].value;
        }
    }
    _length = 0;
    return "";
}

const char *TagReader::getTag(char *jsonBuffer, const char *key)
{
    if (jsonBuffer == nullptr || key == nullptr) {
        _length = 0;
        return "";
    }
    parse(jsonBuffer);
    return getValue(key);
}

const string TagReader::getTag(const string *JSON_Str, const char *value)
{
    if (JSON_Str == nullptr || value == nullptr) {
        _length = 0;
        return string("");
    }

    // Keep compatibility with the old API while routing JSON parsing through
    // the in-place char* implementation.
    std::vector<char> buffer(JSON_Str->begin(), JSON_Str->end());
    buffer.push_back('\0');
    return string(getTag(buffer.data(), value));
}
