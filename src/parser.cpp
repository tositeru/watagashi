#include "parser.h"

#include <iostream>
#include <fstream>

#include <boost/utility/string_view.hpp>
#include "utility.h"

using namespace std;

namespace watagashi::parser
{

static bool isSpace(char c)
{
    return c == ' ' || c == '\t';
}


void parse(boost::filesystem::path const& filepath)
{
    auto source = readFile(filepath);
    parse(source);
}

int calLevel(boost::string_view const& unitIndent, boost::string_view const& indent)
{
    // !!! Attention Precondition !!!
    // for(auto& u : unitIndent) { 
    //   assert(isSpace(u));
    // }
    // for(auto& u : indent) { 
    //   assert(isSpace(u));
    // }

    if (unitIndent.empty()) {
        return -1;
    }

    if ( 0 != (indent.size() % unitIndent.size()) ) {
        return -1;
    }

    int level = 0;
    auto const unitSize = sizeof(unitIndent[0]) * unitIndent.size();
    for (auto i = 0u; i < indent.size(); i += unitIndent.size()) {
        if (0 != memcmp(&indent[i], &unitIndent[0], unitSize)) {
            return -1;
        }
        ++level;
    }
    return level;
}

class Line
{
public:
    Line(char const* source_, size_t head_, size_t tail)
        : mSource(source_)
        , mHead(head_)
        , mLength(tail - head_)
    {}

    void resize(size_t headOffset, size_t tailOffset)
    {
        this->mHead += headOffset;
        this->mLength = this->mLength - std::min(this->mLength, tailOffset+headOffset);
    }

    char get(size_t pos)const { return *this->getPointer(pos); }
    char rget(size_t pos)const { return *this->rgetPointer(pos); }

    char const* getPointer(size_t pos) const
    {
        pos = std::min(pos, this->mLength);
        return &this->mSource[this->mHead + pos];
    }

    char const* rgetPointer(size_t pos)const
    {
        pos = std::min(pos + 1, this->mLength);
        auto p = this->mHead + this->mLength - pos;
        return &this->mSource[p];
    }

    bool isEndLine(size_t pos)const
    {
        return this->mLength <= pos;
    }

    size_t getNextLineHead()const
    {
        return this->mHead + this->mLength + 1;
    }
    
    size_t length()const { return this->mLength; }
    boost::string_view string() const {
        return boost::string_view(&this->mSource[this->mHead], this->mLength);
    }

    size_t incrementPos(size_t start, std::function<bool(Line const& line, size_t p)> continueLoop)const
    {
        auto p = start;
        for (; !this->isEndLine(p) && continueLoop(*this, p); ++p) {}
        return p;
    }

private:
    char const* mSource;
    size_t mHead;
    size_t mLength;

};

class Parser
{
public:
    Parser(char const* source_, size_t length_)
        : mPos(0)
        , mRow(0)
        , mSource(source_)
        , mLength(length_)
        , mLevel(0)
    {}

    //
    //  must implement functions
    //

    bool isEof()const
    {
        return this->mLength <= this->mPos;
    }

    Line getLine(bool doGoNextLine)
    {
        auto result = Line(this->mSource, this->mPos, this->getLineTail());
        if (doGoNextLine) {
            this->mPos = result.getNextLineHead();
            ++this->mRow;
        }
        return result;
    }

    size_t row()const { return this->mRow; }

    void setUnitOfIndent(boost::string_view const& indent)
    {
        this->mUnitOfIndent = indent.to_string();
        this->mLevel = 1;
    }

    int level()const { return this->mLevel; }

    void setLevel(int level)
    {
        if (level <= 0) {
            this->mUnitOfIndent.clear();
        }
        this->mLevel = level;
    }

    int calLevel(boost::string_view const& indent)const
    {
        return watagashi::parser::calLevel(this->mUnitOfIndent, indent);
    }

    //
    // other
    //

    size_t getLineTail()const
    {
        auto i = this->mPos;
        for (; i < this->mLength; ++i) {
            if ('\n' == this->mSource[i]) {
                break;
            }
        }
        return i;
    }

    Line getLine()const
    {
        return Line(this->mSource, this->mPos, this->getLineTail());
    }

    void goNextLine()
    {
        this->mPos = this->getLineTail() + 1;
        ++this->mRow;
    }

private:
    size_t mPos;
    size_t mRow;
    char const* const mSource;
    size_t const mLength;

    std::string mUnitOfIndent;
    int mLevel;
};

static char const COMMENT_CHAR = '#';


void parse(char const* source, std::size_t length)
{
    auto parser = Parser(source, length);

    while (!parser.isEof()) {
        auto rawLine = parser.getLine(true);
        auto workLine = rawLine;
        {//check indent
            auto p = workLine.incrementPos(0, [](auto line, auto p) { return isSpace(line.get(p)); });
            auto indent = boost::string_view(workLine.getPointer(0), p);
            auto level = parser.calLevel(indent);
            if (-1 == level) {
                if (0 == parser.level()) {
                    parser.setUnitOfIndent(indent);
                } else {
                    cerr << parser.row() << ": invalid indent" << endl;
                    continue;
                }
            } else {
                parser.setLevel(level);
            }
            workLine.resize(p, 0);
        }
        {//check comment
            if (COMMENT_CHAR == workLine.get(0)) {
                if (2 <= workLine.length() && COMMENT_CHAR == workLine.get(1)) {
                    //if multiple line comment
                    int p = workLine.incrementPos(2, [](auto line, auto p) { return COMMENT_CHAR == line.get(p); });

                    // TODO
                } else {
                    //if single line comment
                    workLine.resize(0, workLine.length());
                }
            } else if (COMMENT_CHAR == workLine.rget(0)) {
                // check comment at the end of the line.
                // count '#' at the end of line.
                size_t p = workLine.incrementPos(1, [](auto line, auto p) { return COMMENT_CHAR == line.rget(p); });

                auto const KEYWARD_COUNT = p;
                // search pair '###...'
                size_t count = 0;
                for (count = 0; !workLine.isEndLine(p); ++p) {
                    if ( count        == KEYWARD_COUNT
                      && COMMENT_CHAR != workLine.rget(p)) {
                        break;
                    }

                    count = (COMMENT_CHAR == workLine.rget(p)) ? count + 1 : 0;
                }
                if (count == KEYWARD_COUNT) {
                    // remove space characters before comment.
                    p = workLine.incrementPos(p, [](auto line, auto p) { return isSpace(line.rget(p)); });
                    workLine.resize(0, p);
                }
            }
        }
        {//parse syntax

        }
        cout << parser.row() << "," << parser.level() << ":" << workLine.string() << endl;
    }
}

}