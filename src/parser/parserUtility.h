#pragma once

#include <sstream>
#include <string>
#include <list>
#include <boost/utility/string_view.hpp>

namespace parser
{

std::list<std::string> toStringList(std::list<boost::string_view> const& list);
double toDouble(std::string const& str, bool& isSuccess);
std::string toNameString(std::list<std::string> const& nestName);
std::string toNameString(std::list<boost::string_view> const& nestName);

bool isSpace(char const* c);
bool isNameChar(char const* c);
bool isParentOrderAccessorChar(char const* c);
bool isChildOrderAccessorString(boost::string_view const& str);
bool isArrayElementSeparater(char const* c);
bool isExplicitStringArrayElementSeparater(boost::string_view const& str);
bool isReference(std::string const& str);

enum class CommentType
{
    None,
    SingleLine,
    MultiLine,
    EndOfLine,
};
bool isCommentChar(char const* c);

enum class OperatorType
{
    Unknown,
    Is,
    Are,
    Judge,
    Deny,
    Copy,
    Extend,
    PushBack,
    Remove,
    DefineFunction
};
OperatorType toOperatorType(boost::string_view const& str);
boost::string_view const toString(OperatorType type);

enum class MemberDefinedOperatorType
{
    Unknown,
    ByDefault,
};

MemberDefinedOperatorType toMemberDefinedOperatorType(boost::string_view const& str);
boost::string_view const toString(MemberDefinedOperatorType type);

enum class CompareOperator
{
    Unknown,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual
};
CompareOperator toCompareOperatorType(boost::string_view const& str);
boost::string_view const toString(CompareOperator type);

enum class LogicOperator
{
    Unknown,
    And,
    Or,
    Continue,
};
LogicOperator toLogicOperatorType(boost::string_view const& str);
boost::string_view const toString(LogicOperator type);
bool isContinueLogicOperatorChar(char const* c);

enum class Statement {
    Unknown,
    EmptyLine,
    Branch,
    Unless,
    Local,
};
Statement toStatementType(boost::string_view const& str);
boost::string_view const toString(Statement type);

enum class DefineFunctionOperator {
    Unknown,
    ToPass,
    ToCapture,
    WithContents,
};
DefineFunctionOperator toDefineFunctionOperatorType(boost::string_view const& str);
boost::string_view const toString(DefineFunctionOperator type);

enum class ArgumentOperator
{
    Unknown,
    Is,
    ByDefault,
};
ArgumentOperator toArgumentOperator(boost::string_view const& str);
boost::string_view const toString(ArgumentOperator type);

enum class CallFunctionOperator
{
    Unknown,
    ByUsing,
    PassTo,
};
CallFunctionOperator toCallFunctionOperaotr(boost::string_view const& str);
boost::string_view const toString(CallFunctionOperator type);

struct MakeExceptionCommand {};

extern MakeExceptionCommand const MAKE_EXCEPTION;

template<typename Exception>
class MakeException
{
public:

    template<typename U>
    MakeException& operator<<(U &&x)
    {
        this->mMessage << std::forward<U>(x);
        return *this;
    }

    template<typename U>
    MakeException& operator<<(U const&x)
    {
        this->mMessage << x;
        return *this;
    }

    Exception operator<<(MakeExceptionCommand &&)
    {
        return this->makeException();
    }

    Exception operator<<(MakeExceptionCommand const&)
    {
        return this->makeException();
    }

    Exception makeException()const
    {
        return Exception(mMessage.str());
    }

private:
    std::stringstream mMessage;

};

class ParserException : public std::exception
{
public:
    ParserException(std::string const& message)
        : mMessage(message)
    {}

    virtual ~ParserException() {}

    const char* what() const noexcept
    {
        return this->mMessage.c_str();
    }

private:
    std::string mMessage;
};

class SyntaxException : public ParserException
{
public:
    SyntaxException(std::string const& message)
        : ParserException("!!syntax error!! " + message)
    {}

};

class ScopeSearchingException : public ParserException
{
public:
    ScopeSearchingException(std::string const& message)
        : ParserException("!!scope seatching error!! " + message)
    {}

};

class DefinedObjectException : public ParserException
{
public:
    DefinedObjectException(std::string const& message)
        : ParserException("!!defined object error!! " + message)
    {}

};

class BooleanException : public ParserException
{
public:
    BooleanException(std::string const& message)
        : ParserException("!!boolean error!! " + message)
    {}

};

class FatalException : public ParserException
{
public:
    FatalException(std::string const& message)
        : ParserException("!!fatal error!! " + message)
    {}

};

}

namespace std
{

template<> struct hash<boost::string_view>
{
    size_t operator()(boost::string_view const& right) const {
        //FNV-1a hashes
        size_t v = 12438926392u;
        for (auto i = 0u; i < right.length(); ++i) {
            v ^= static_cast<size_t>(right[i]);
            v *= 547302934u;
        }
        return v;
    }
};

}
