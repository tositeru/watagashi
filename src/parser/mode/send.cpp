#include "send.h"

#include "../enviroment.h"

#include"normal.h"

namespace parser
{

IParseMode::Result SendParseMode::parse(Enviroment& env, Line& line)
{
    env.pushMode(std::make_shared<NormalParseMode>());
    parseArrayElement(env, line, 0);
    return Result::Continue;
}

}