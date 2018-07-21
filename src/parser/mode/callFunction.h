#pragma once

#include "../parseMode.h"

namespace parser
{

class CallFunctionParseMode final : public IParseMode
{
public:
    Result parse(Enviroment& env, Line& line);

private:
    Result parseDefault(Enviroment& env, Line line);
    Result parseArguments(Enviroment& env, Line line);
    Result parseReturnValues(Enviroment& env, Line line);
};


}