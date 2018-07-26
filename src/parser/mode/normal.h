#pragma once

#include "../parseMode.h"

namespace parser
{

class NormalParseMode final : public IParseMode
{
public:
    Result parse(Enviroment& env, Line& line);

};

}