#include "source.h"

#include "line.h"

namespace parser
{

Source::Source(char const* source_, size_t length_)
    : mPos(0)
    , mRow(0)
    , mSource(source_)
    , mLength(length_)
{}

//
//  must implement functions
//

bool Source::isEof()const
{
    return this->mLength <= this->mPos;
}

Line Source::getLine(bool doGoNextLine)
{
    auto result = Line(this->mSource, this->mPos, this->getLineEnd());
    if (doGoNextLine) {
        this->mPos = result.getNextLineHead();
        ++this->mRow;
    }
    return result;
}

size_t Source::row()const { return this->mRow; }

//
// other
//

size_t Source::getLineEnd()const
{
    auto i = this->mPos;
    for (; i < this->mLength; ++i) {
        if ('\n' == this->mSource[i]) {
            break;
        }
    }
    return i;
}

Line Source::getLine()const
{
    return Line(this->mSource, this->mPos, this->getLineEnd());
}

void Source::goNextLine()
{
    this->mPos = this->getLineEnd() + 1;
    ++this->mRow;
}

}