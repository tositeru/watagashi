#include "line.h"

#include "parserUtility.h"

namespace parser
{

Line::Line(char const* source_, size_t head_, size_t tail)
    : mSource(source_)
    , mHead(head_)
    , mLength(tail - head_)
{}

void Line::resize(size_t headOffset, size_t tailOffset)
{
    this->mHead += headOffset;
    this->mLength = this->mLength - std::min(this->mLength, tailOffset + headOffset);
}

char const* Line::get(size_t pos) const
{
    pos = std::min(pos, this->mLength);
    return &this->mSource[this->mHead + pos];
}

char const* Line::rget(size_t pos)const
{
    pos = std::min(pos + 1, this->mLength);
    auto p = this->mHead + this->mLength - pos;
    return &this->mSource[p];
}

bool Line::isEndLine(size_t pos)const
{
    return this->mLength <= pos;
}

size_t Line::getNextLineHead()const
{
    return this->mHead + this->mLength + 1;
}

size_t Line::length()const { return this->mLength; }
boost::string_view Line::string() const {
    return boost::string_view(&this->mSource[this->mHead], this->mLength);
}

size_t Line::incrementPos(size_t start, std::function<bool(Line const& line, size_t p)> continueLoop)const
{
    auto p = start;
    for (; !this->isEndLine(p) && continueLoop(*this, p); ++p) {}
    return p;
}

bool Line::find(size_t start, std::function<bool(Line const& line, size_t p)> didFound)const
{
    bool result = false;
    for (auto p = start; !this->isEndLine(p) && !(result = didFound(*this, p)); ++p) {}
    return result;
}

boost::string_view Line::getIndent()const
{
    auto p = this->incrementPos(0, [](auto line, auto p) { return isSpace(line.get(p)); });
    return boost::string_view(this->get(0), p);
}

}