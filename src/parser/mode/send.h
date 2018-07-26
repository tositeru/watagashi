#pragma once

#include "../parseMode.h"

namespace parser
{

class SendParseMode : public IParseMode
{
public:
    Result parse(Enviroment& env, Line& line);

};

}