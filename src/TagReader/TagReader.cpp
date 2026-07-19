/**
 * Author: James Brown 2018
 * Crawls JSON looking for tags without extra allocation.
 */

#include "TagReader.h"
#include <string.h>

using std::string;

typedef enum STATE_DEF
{
    KEY,
    DELIMITER,
    VALUE,
    DEREFERENCE,
    END_VALUE
} READ_STATE;

TagReader::TagReader()
{
    _length = 0;
}

const string TagReader::getTag(const string *JSON_Str, const char *value)
{
    if (JSON_Str == nullptr || value == nullptr) {
        _length = 0;
        return string("");
    }

    size_t index = JSON_Str->find(value);
    if (index == string::npos) {
        _length = 0;
        return string("");
    }

    READ_STATE read_state = KEY;
    READ_STATE old_state = KEY;
    size_t startIndex = 0;
    _length = 0;

    while (index < JSON_Str->length() && read_state != END_VALUE)
    {
        const char c = JSON_Str->at(index);

        switch (read_state)
        {
        case KEY:
            if (c == '\"' || c == ':') read_state = DELIMITER;
            break;
        case DELIMITER:
            if (c != '\"' && c != ':') { read_state = VALUE; index--; }
            break;
        case VALUE:
            if (_length == 0) {
                startIndex = index;
            }
            if (c == '\"') {
                read_state = END_VALUE;
            } else {
                _length++;
            }
            break;
        case DEREFERENCE:
            read_state = old_state;
            break;
        case END_VALUE:
            break;
        }

        if (c == '\\') {
            old_state = read_state;
            read_state = DEREFERENCE;
        }
        index++;
    }

    return JSON_Str->substr(startIndex, _length);
}
