#ifndef WEB3_TAG_READER_H
#define WEB3_TAG_READER_H

#include <stddef.h>
#include <string>
#include <vector>

class TagReader
{
public:
    typedef struct {
        char *key;
        char *value;
    } Tag;

    TagReader();

    // Compatibility wrapper: returns a copy for existing std::string callers.
    const std::string getTag(const std::string *JSON_Str, const char *value);

    // In-place parser. The JSON buffer is modified by inserting '\0' terminators.
    // Returned pointers refer to memory inside jsonBuffer; keep that buffer alive.
    const char *getTag(char *jsonBuffer, const char *key);
    size_t parse(char *jsonBuffer);
    const char *getValue(const char *key) const;
    const std::vector<Tag>& tags() const { return _tags; }

    size_t length() const { return _length; }

private:
    static char *skipWhitespace(char *p);
    static char *parseString(char *p, char **out);
    static char *parsePrimitive(char *p, char **out);
    static bool equals(const char *a, const char *b);

    mutable size_t _length;
    std::vector<Tag> _tags;
};

#endif // WEB3_TAG_READER_H
