#pragma once

#include "../parseMode.h"

namespace parser
{

class BooleanParseMode final : public IParseMode
{
public:
    Result parse(Enviroment& env, Line& line);

};

}