#pragma once

#include "../parseMode.h"

namespace parser
{

class ArrayAccessorParseMode : public IParseMode
{
public:
    Result parse(Enviroment& env, Line& line);

};

}