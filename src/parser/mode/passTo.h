#pragma once

#include "../parseMode.h"

namespace parser
{

class PassToParseMode : public IParseMode
{
public:
    Result parse(Enviroment& env, Line& line);

};

}