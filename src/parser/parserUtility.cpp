#include "parserUtility.h"

#include <cctype>
#include <unordered_map>

#include <boost/bimap.hpp>
#include <boost/assign.hpp>

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

bool isArrayElementSeparater(char const* c)
{
    return ',' == *c;
}


using OperationBimap = boost::bimap<boost::string_view, OperatorType>;
static const OperationBimap operationBimap = boost::assign::list_of<OperationBimap::relation>
    ("is", OperatorType::Is )
    ("are", OperatorType::Are)
    ("copy", OperatorType::Copy )
    ("extend", OperatorType::Extend)
    ("push_back", OperatorType::PushBack)
    ("remove", OperatorType::Remove);

OperatorType toOperatorType(boost::string_view const& str)
{
    auto it = operationBimap.left.find(str);
    return operationBimap.left.end() == it
        ? OperatorType::Unknown
        : it->get_right();
}

boost::string_view const toString(OperatorType type)
{
    static char const* UNKNOWN = "(unknown)";
    auto it = operationBimap.right.find(type);
    return operationBimap.right.end() == it
        ? UNKNOWN
        : it->get_left();
}

}