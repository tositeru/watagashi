#pragma once

#include <vector>

#include "source.h"
#include "indent.h"
#include "parseMode.h"

namespace parser
{

struct Enviroment
{
    Source source;
    Indent indent;
    std::vector<std::shared_ptr<IParseMode>> modeStack;

    explicit Enviroment(char const* source_, std::size_t length);

    void pushMode(std::shared_ptr<IParseMode> pMode);
    void popMode();

    std::shared_ptr<IParseMode> currentMode();

};

}