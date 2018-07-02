#pragma once

#include "../parseMode.h"

namespace parser
{

class MultiLineCommentParseMode final : public IParseMode
{
public:
    MultiLineCommentParseMode(int keywardCount);
    Result parse(Enviroment& parser, Line& line)override;

private:
    int mKeywardCount;
};

}