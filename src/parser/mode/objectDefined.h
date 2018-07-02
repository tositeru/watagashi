#pragma once

#include "../parseMode.h"

namespace parser
{

class ObjectDefinedParseMode final : public IParseMode
{
public:
    Result parse(Enviroment& env, Line& line);

};


}