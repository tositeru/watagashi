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
    Copy,
    Extend,
    PushBack,
    Remove,
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

bool isSpace(char const* c);
bool isNameChar(char const* c);
bool isParentOrderAccessorChar(char const* c);
bool isChildOrderAccessorString(boost::string_view const& str);
bool isArrayElementSeparater(char const* c);
bool isExplicitStringArrayElementSeparater(boost::string_view const& str);
bool isReference(std::string const& str);

//class ErrorHandle
//{
//private:
//    friend class MakeErrorHandle;
//    explicit ErrorHandle(size_t row, std::string&& message)
//        : mRow(row)
//        , mMessage(message)
//    {}
//
//public:
//    ErrorHandle(ErrorHandle const&) = default;
//    ErrorHandle& operator=(ErrorHandle const&) = default;
//    ErrorHandle(ErrorHandle &&) = default;
//    ErrorHandle& operator=(ErrorHandle &&) = default;
//
//    ErrorHandle(size_t row, ErrorHandle && right)
//        : mRow(row)
//        , mMessage(std::move(right.mMessage))
//    {}
//
//    ErrorHandle()
//        : mRow(static_cast<size_t>(-1))
//        , mMessage()
//    {}
//
//    explicit operator bool() const
//    {
//        return !this->mMessage.empty();
//    }
//
//    size_t row()const { return this->mRow; }
//    std::string message()const {
//        if (0 != this->mRow) {
//            return std::to_string(this->mRow) + ": " + this->mMessage;
//        } else {
//            return this->mMessage;
//        }
//    }
//
//private:
//    size_t mRow;
//    std::string mMessage;
//};
//
//class MakeErrorHandle
//{
//public:
//    explicit MakeErrorHandle(size_t row)
//        : mRow(row)
//    {}
//
//    operator ErrorHandle() const
//    {
//        return ErrorHandle(this->mRow, this->mMessage.str());
//    }
//
//    template<typename U>
//    MakeErrorHandle& operator<<(U &&x)
//    {
//        this->mMessage << std::forward<U>(x);
//        return *this;
//    }
//
//    template<typename U>
//    MakeErrorHandle& operator<<(U const&x)
//    {
//        this->mMessage << x;
//        return *this;
//    }
//
//private:
//    size_t mRow;
//    std::stringstream mMessage;
//};

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

    Exception operator<<(MakeExceptionCommand &&x)
    {
        return this->makeException();
    }

    Exception operator<<(MakeExceptionCommand const&x)
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
