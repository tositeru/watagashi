#pragma once

#include "../parseMode.h"

namespace parser
{

class NormalParseMode final : public IParseMode
{
public:
    void parse(Enviroment& env, Line& line);

};

}