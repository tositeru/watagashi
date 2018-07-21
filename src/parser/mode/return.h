#pragma once

#include "../parseMode.h"

namespace parser
{

class ReturnParseMode : public IParseMode
{
public:
    Result parse(Enviroment& env, Line& line);

};

}