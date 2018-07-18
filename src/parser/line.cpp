#include "line.h"

#include "parserUtility.h"

namespace parser
{

Line::Line(char const* source_, size_t begin_, size_t end)
    : mSource(source_)
    , mBegin(begin_)
    , mLength(end - begin_)
{}

void Line::resize(size_t beginOffset, size_t endOffset)
{
    this->mBegin += beginOffset;
    this->mLength = this->mLength - std::min(this->mLength, endOffset + beginOffset);
}

char const* Line::get(size_t pos) const
{
    pos = std::min(pos, this->mLength);
    return &this->mSource[this->mBegin + pos];
}

char const* Line::rget(size_t pos)const
{
    pos = std::min(pos + 1, this->mLength);
    auto p = this->mBegin + this->mLength - pos;
    return &this->mSource[p];
}

bool Line::isEndLine(size_t pos)const
{
    return this->mLength <= pos;
}

size_t Line::getNextLineHead()const
{
    return this->mBegin + this->mLength + 1;
}

size_t Line::length()const { return this->mLength; }
boost::string_view Line::string_view() const {
    return boost::string_view(&this->mSource[this->mBegin], this->mLength);
}

boost::string_view Line::substr(size_t start, size_t length)const
{
    return boost::string_view(&this->mSource[this->mBegin + start], std::min(length, this->mLength));
}

size_t Line::incrementPos(size_t start, std::function<bool(Line const& line, size_t p)> continueLoop)const
{
    auto p = start;
    for (; !this->isEndLine(p) && continueLoop(*this, p); ++p) {}
    return p;
}

size_t Line::decrementPos(size_t start, std::function<bool(Line const& line, size_t p)> continueLoop)const
{
    auto p = start;
    for (; 0 < p && continueLoop(*this, p); --p) {}
    return p;
}

bool Line::find(size_t start, std::function<bool(Line const& line, size_t p)> didFound)const
{
    bool result = false;
    for (auto p = start; !this->isEndLine(p) && !result; ++p) {
        result = didFound(*this, p);
    }
    return result;
}

size_t Line::skipSpace(size_t start)const
{
    return this->incrementPos(start, [](auto line, auto p) { return isSpace(line.get(p)); });
}

size_t Line::skipSpaceInOppositeDirection(size_t start)const
{
    return this->decrementPos(start, [](auto line, auto p) {
        return isSpace(line.get(p));
    });
}

std::tuple<size_t, size_t> Line::getRangeSeparatedBySpace(size_t start)const
{
    auto s = this->skipSpace(start);
    auto e = this->incrementPos(s, [](auto line, auto p) { return !isSpace(line.get(p));  });
    return {s, e};
}

boost::string_view Line::getIndent()const
{
    auto p = this->incrementPos(0, [](auto line, auto p) { return isSpace(line.get(p)); });
    return boost::string_view(this->get(0), p);
}

}