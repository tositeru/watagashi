#pragma once

namespace parser
{

struct Enviroment;
class Line;

class IParseMode
{
public:
    virtual ~IParseMode() {}

    virtual void parse(Enviroment& parser, Line& line) = 0;
};

}