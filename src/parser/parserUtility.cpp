#include "parserUtility.h"

#include <cctype>

namespace parser
{

inline static char const COMMENT_CHAR = '#';

bool isCommentChar(char const* c)
{
    return '#' == *c;
}

bool isSpace(char const* c)
{
    return *c == ' ' || *c == '\t';
}

bool isParentOrderAccessorChar(char const* c)
{
    return '.' == *c;
}

bool isChildOrderAccessorString(boost::string_view const& str)
{
    if (str.length() < 2) { return false; }
    static char const* keyward = "in";
    return keyward[0] == str[0] && keyward[1] == str[1];
}

bool isNameChar(char const* c)
{
    return std::isalnum(*c)
        || '_' == *c
        || '?' == *c
        || '!' == *c;
}

}