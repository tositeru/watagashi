#include "multiLineComment.h"

#include "../enviroment.h"
#include "../line.h"
#include "../parserUtility.h"

namespace parser
{

MultiLineCommentParseMode::MultiLineCommentParseMode(int keywardCount)
    : mKeywardCount(keywardCount)
{}

void MultiLineCommentParseMode::parse(Enviroment& parser, Line& line)
{
    size_t count = 0;
    size_t p = 0;
    for (count = 0; !line.isEndLine(p); ++p) {
        if (count == this->mKeywardCount
            && false == isCommentChar(line.get(p))) {
            break;
        }
        count = isCommentChar(line.rget(p)) ? count + 1 : 0;
    }
    if (count == this->mKeywardCount) {
        parser.popMode();
    }
}

}