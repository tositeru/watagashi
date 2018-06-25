#pragma once

#include <sstream>
#include <string>

namespace parser
{

enum class CommentType
{
    None,
    SingleLine,
    MultiLine,
    EndOfLine,
};

bool isCommentChar(char const* c);
bool isSpace(char const* c);

class ErrorHandle
{
private:
    friend class MakeErrorHandle;
    explicit ErrorHandle(size_t row, std::string&& message)
        : mRow(row)
        , mMessage(message)
    {}

public:
    ErrorHandle(ErrorHandle const&) = default;
    ErrorHandle& operator=(ErrorHandle const&) = default;
    ErrorHandle(ErrorHandle &&) = default;
    ErrorHandle& operator=(ErrorHandle &&) = default;

    ErrorHandle()
        : mRow(static_cast<size_t>(-1))
        , mMessage()
    {}

    explicit operator bool() const
    {
        return !this->mMessage.empty();
    }

    size_t row()const { return this->mRow; }
    std::string message()const {
        return std::to_string(this->mRow) + ": " + this->mMessage;
    }

private:
    size_t mRow;
    std::string mMessage;
};

class MakeErrorHandle
{
public:
    explicit MakeErrorHandle(size_t row)
        : mRow(row)
    {}

    operator ErrorHandle() const
    {
        return ErrorHandle(this->mRow, this->mMessage.str());
    }

    template<typename U>
    MakeErrorHandle& operator<<(U &&x)
    {
        this->mMessage << std::forward<U>(x);
        return *this;
    }

    template<typename U>
    MakeErrorHandle& operator<<(U const&x)
    {
        this->mMessage << x;
        return *this;
    }

private:
    size_t mRow;
    std::stringstream mMessage;
};

}