#include "parserUtility.h"

#include <cctype>
#include <unordered_map>

#include <boost/bimap.hpp>
#include <boost/assign.hpp>

namespace parser
{

MakeExceptionCommand const MAKE_EXCEPTION;

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
static OperationBimap const operationBimap = boost::assign::list_of<OperationBimap::relation>
    ("is", OperatorType::Is )
    ("are", OperatorType::Are)
    ("judge", OperatorType::Judge)
    ("deny", OperatorType::Deny)
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
static MemberDefinedOperationBimap const memberDefinedOperationBimap = boost::assign::list_of<MemberDefinedOperationBimap::relation>
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

using CompareOperatorBimap = boost::bimap<boost::string_view, CompareOperator>;
static CompareOperatorBimap const compareOperationBimap = boost::assign::list_of<CompareOperatorBimap::relation>
    ("equal", CompareOperator::Equal)
    ("not_equal", CompareOperator::NotEqual)
    ("less", CompareOperator::Less)
    ("less_equal", CompareOperator::LessEqual)
    ("greater", CompareOperator::Greater)
    ("greater_equal", CompareOperator::GreaterEqual);

CompareOperator toCompareOperatorType(boost::string_view const& str)
{
    auto it = compareOperationBimap.left.find(str);
    return compareOperationBimap.left.end() == it
        ? CompareOperator::Unknown
        : it->get_right();
}

boost::string_view const toString(CompareOperator type)
{
    static char const* UNKNOWN = "(unknown)";
    auto it = compareOperationBimap.right.find(type);
    return compareOperationBimap.right.end() == it
        ? UNKNOWN
        : it->get_left();
}

using LogicOperatorBimap = boost::bimap<boost::string_view, LogicOperator>;
static LogicOperatorBimap const logicOperationBimap = boost::assign::list_of<LogicOperatorBimap::relation>
    ("and", LogicOperator::And)
    ("or", LogicOperator::Or)
    (",", LogicOperator::Continue);

LogicOperator toLogicOperatorType(boost::string_view const& str)
{
    auto it = logicOperationBimap.left.find(str);
    return logicOperationBimap.left.end() == it
        ? LogicOperator::Unknown
        : it->get_right();
}

boost::string_view const toString(LogicOperator type)
{
    static char const* UNKNOWN = "(unknown)";
    auto it = logicOperationBimap.right.find(type);
    return logicOperationBimap.right.end() == it
        ? UNKNOWN
        : it->get_left();
}

bool isContinueLogicOperatorChar(char const* c)
{
    return ',' == *c;
}

using StatementBimap = boost::bimap<boost::string_view, Statement>;
static StatementBimap const statementBimap = boost::assign::list_of<StatementBimap::relation>
    ("empty_line", Statement::EmptyLine)
    ("if", Statement::Branch)
    ("unless", Statement::Unless);

Statement toStatementType(boost::string_view const& str)
{
    auto it = statementBimap.left.find(str);
    return statementBimap.left.end() == it
        ? Statement::Unknown
        : it->get_right();
}

boost::string_view const toString(Statement type)
{
    static char const* UNKNOWN = "(unknown)";
    auto it = statementBimap.right.find(type);
    return statementBimap.right.end() == it
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
    char* end;
    auto num = strtod(str.c_str(), &end);
    isSuccess = true;
    if (0 == num) {
        bool isNumber = ('0' == str[0]);
        isNumber |= (2 <= str.size() && ('-' == str[0] && '0' == str[1]));
        isSuccess = isNumber;
    }
    return num;
}

std::string toNameString(std::list<std::string> const& nestName)
{
    std::string fullname = "";
    std::string accesser = "";
    for (auto&& name : nestName) {
        fullname += accesser + name;
        accesser = ".";
    }
    return fullname;
}

std::string toNameString(std::list<boost::string_view> const& nestName)
{
    std::string fullname = "";
    std::string accesser = "";
    for (auto&& name : nestName) {
        fullname += accesser + name.to_string();
        accesser = ".";
    }
    return fullname;
}

bool isReference(std::string const& str)
{
    if ('$' != str[0] && '|' == str[1]) {
        return false;
    }
    auto end = str.find('}', 0);
    return str.size() == end+1;
}

}