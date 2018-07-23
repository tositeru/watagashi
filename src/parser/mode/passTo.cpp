#include "passTo.h"

#include "../enviroment.h"

#include "../line.h"
#include "normal.h"

namespace parser
{

IParseMode::Result PassToParseMode::parse(Enviroment& env, Line& line)
{
    auto endPos = foreachArrayElement(line, 0, [&](auto line) {
        if (env.isEmptyArguments()) {
            return size_t(0);
        }

        auto [nestNameView, nestNameEnd] = parseName(line, 0);
        auto nestName = toStringList(nestNameView);
        auto pValue = env.searchValue(nestName, false, nullptr);
        if (pValue) {
            *pValue = env.moveCurrentHeadArgument();
        } else {
            auto pParentValue = env.searchValue(nestName, true, nullptr);
            if (pParentValue) {
                pParentValue->addMember(nestName.back(), env.moveCurrentHeadArgument());
            } else {
                env.currentScope().value().addMember(nestName.back(), env.moveCurrentHeadArgument());
            }
        }
        return GO_NEXT_ELEMENT;
    });
    return Result::Continue;
}

}