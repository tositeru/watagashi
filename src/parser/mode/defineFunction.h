#pragma once

#include "../parseMode.h"

namespace parser
{

class DefineFunctionParseMode final : public IParseMode
{
public:
    DefineFunctionParseMode();

    Result parse(Enviroment& env, Line& line);

    void resetMode();

private:
    Result parseByDefaultMode(Enviroment& env, Line& line);
    Result parseByPassMode(Enviroment& env, Line& line);
    Result parseByCaptureMode(Enviroment& env, Line& line);
    Result parseByContentsMode(Enviroment& env, Line& line);

private:
    enum class Mode
    {
        Default,
        ToPass,
        ToCapture,
        WithContents,
    };

    Mode mCurrentMode;
};

}
