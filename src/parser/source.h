#pragma once


namespace parser
{

class Line;

class Source
{
public:
    explicit Source(char const* source_, size_t length_);
    //
    //  must implement functions
    //

    bool isEof()const;
    Line getLine(bool doGoNextLine);
    size_t row()const;

    //
    // other
    //

    size_t getLineEnd()const;
    Line getLine()const;
    void goNextLine();

private:
    size_t mPos;
    size_t mRow;
    char const* const mSource;
    size_t const mLength;
};

}
