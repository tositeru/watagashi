#pragma once

#include <string>
#include <functional>
#include <boost/utility/string_view.hpp>

namespace parser
{

class Line
{
public:
    Line(char const* source_, size_t begin_, size_t end);

    void resize(size_t beginOffset, size_t endOffset);

    char const* get(size_t pos) const;
    char const* rget(size_t pos)const;

    bool isEndLine(size_t pos)const;

    size_t getNextLineHead()const;
    size_t length()const;
    boost::string_view string_view() const;
    boost::string_view substr(size_t start, size_t length)const;

    size_t incrementPos(size_t start, std::function<bool(Line const& line, size_t p)> continueLoop)const;
    bool find(size_t start, std::function<bool(Line const& line, size_t p)> didFound)const;
    size_t skipSpace(size_t start)const;

    boost::string_view getIndent()const;

private:
    char const* mSource;
    size_t mBegin;
    size_t mLength;

};

}