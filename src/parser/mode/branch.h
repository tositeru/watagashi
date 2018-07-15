#pragma once

#include "../parseMode.h"

namespace parser
{

class BranchParseMode final : public IParseMode
{
public:
    Result parse(Enviroment& env, Line& line);

};

}