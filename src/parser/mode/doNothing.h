#pragma once

#include "../parseMode.h"

namespace parser
{

class DoNothingParseMode : public IParseMode
{
public:
    Result parse(Enviroment& /*env*/, Line& /*line*/) {
        return Result::NextLine;
    }
};

}