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
    ("remove", OperatorType::Remove)
    ("define_function", OperatorType::DefineFunction)
    ("receive", OperatorType::Receive);

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
    ("if", Statement::If)
    ("unless", Statement::Unless)
    ("local", Statement::Local)
    ("send", Statement::Send)
    ("pass_to", Statement::PassTo)
    ("finish", Statement::Finish);

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

using DefineFunctionOperatorBimap = boost::bimap<boost::string_view, DefineFunctionOperator>;
static DefineFunctionOperatorBimap const defineFunctionOperatorBimap = boost::assign::list_of<DefineFunctionOperatorBimap::relation>
    ("to_receive", DefineFunctionOperator::ToReceive)
    ("to_capture", DefineFunctionOperator::ToCapture)
    ("with_contents", DefineFunctionOperator::WithContents);

DefineFunctionOperator toDefineFunctionOperatorType(boost::string_view const& str)
{
    auto it = defineFunctionOperatorBimap.left.find(str);
    return defineFunctionOperatorBimap.left.end() == it
        ? DefineFunctionOperator::Unknown
        : it->get_right();
}

boost::string_view const toString(DefineFunctionOperator type)
{
    static char const* UNKNOWN = "(unknown)";
    auto it = defineFunctionOperatorBimap.right.find(type);
    return defineFunctionOperatorBimap.right.end() == it
        ? UNKNOWN
        : it->get_left();
}

using ArgumentOperatorBimap = boost::bimap<boost::string_view, ArgumentOperator>;
static ArgumentOperatorBimap const argumentOperatorBimap = boost::assign::list_of<ArgumentOperatorBimap::relation>
    ("is", ArgumentOperator::Is)
    ("by_default", ArgumentOperator::ByDefault);

ArgumentOperator toArgumentOperator(boost::string_view const& str)
{
    auto it = argumentOperatorBimap.left.find(str);
    return argumentOperatorBimap.left.end() == it
        ? ArgumentOperator::Unknown
        : it->get_right();
}

boost::string_view const toString(ArgumentOperator type)
{
    static char const* UNKNOWN = "(unknown)";
    auto it = argumentOperatorBimap.right.find(type);
    return argumentOperatorBimap.right.end() == it
        ? UNKNOWN
        : it->get_left();
}

using CallFunctionOperatorBimap = boost::bimap<boost::string_view, CallFunctionOperator>;
static CallFunctionOperatorBimap const callFunctionOperatorBimap = boost::assign::list_of<CallFunctionOperatorBimap::relation>
    ("by_using", CallFunctionOperator::ByUsing)
    ("pass_to", CallFunctionOperator::PassTo);

CallFunctionOperator toCallFunctionOperaotr(boost::string_view const& str)
{
    auto it = callFunctionOperatorBimap.left.find(str);
    return callFunctionOperatorBimap.left.end() == it
        ? CallFunctionOperator::Unknown
        : it->get_right();
}

boost::string_view const toString(CallFunctionOperator type)
{
    static char const* UNKNOWN = "(unknown)";
    auto it = callFunctionOperatorBimap.right.find(type);
    return callFunctionOperatorBimap.right.end() == it
        ? UNKNOWN
        : it->get_left();
}

using ArrayAccessorBimap = boost::bimap<boost::string_view, size_t>;
static std::unordered_map<boost::string_view, size_t> const arrayAccessorHash = {
    { "all", 0 },
    { "first",  1 },
    { "second", 2 },
    { "third",  3 },
    { "fourth", 4 },
    { "fifth",  5 },
    { "sixth",  6 },
    { "seventh", 7 },
    { "eighth", 8 },
    { "ninth", 9 },
    { "tenth", 10 },
};

size_t toArrayIndex(boost::string_view const& str)
{
    auto it = arrayAccessorHash.find(str);
    return arrayAccessorHash.end() == it
        ? -1
        : it->second;
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
    if ('$' != str[0] && '{' != str[1]) {
        return false;
    }
    auto end = str.find('}', 0);
    return str.size() == end+1;
}

}