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
    static std::string const keyward = "in";
    if (str.length() < keyward.size()) { return false; }
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

bool isExplicitStringArrayElementSeparater(boost::string_view const& str)
{
    static std::string const keyward = "\\,";
    if (str.length() < keyward.size()) { return false; }
    return keyward[0] == str[0] && keyward[1] == str[1];
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

using MemberDefinedOperationBimap = boost::bimap<boost::string_view, MemberDefinedOperatorType>;
static const MemberDefinedOperationBimap memberDefinedOperationBimap = boost::assign::list_of<MemberDefinedOperationBimap::relation>
    ("by_default", MemberDefinedOperatorType::ByDefault);

MemberDefinedOperatorType toMemberDefinedOperatorType(boost::string_view const& str)
{
    auto it = memberDefinedOperationBimap.left.find(str);
    return memberDefinedOperationBimap.left.end() == it
        ? MemberDefinedOperatorType::Unknown
        : it->get_right();
}

boost::string_view const toString(MemberDefinedOperatorType type)
{
    static char const* UNKNOWN = "(unknown)";
    auto it = memberDefinedOperationBimap.right.find(type);
    return memberDefinedOperationBimap.right.end() == it
        ? UNKNOWN
        : it->get_left();
}

std::list<std::string> toStringList(std::list<boost::string_view> const& list)
{
    std::list<std::string> result;
    for (auto& view : list) {
        result.push_back(view.to_string());
    }
    return result;
}

double toDouble(std::string const& str, bool& isSuccess)
{
    char* tail;
    auto num = strtod(str.c_str(), &tail);
    isSuccess = true;
    if (0 == num) {
        bool isNumber = ('0' == str[0]);
        isNumber |= (2 <= str.size() && ('-' == str[0] && '0' == str[1]));
        isSuccess = isNumber;
    }
    return num;
}

}