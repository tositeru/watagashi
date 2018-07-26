#include "arrayAccessor.h"

#include "../parserUtility.h"
#include "../enviroment.h"
#include "../scope.h"
#include "../line.h"

#include "callFunction.h"

namespace parser
{

IParseMode::Result ArrayAccessorParseMode::parse(Enviroment& env, Line& line)
{
    auto& arrayAccessorScope = dynamic_cast<ArrayAccessorScope&>(env.currentScope());
    if (arrayAccessorScope.doAccessed()) {
        return Result::Continue;
    }

    size_t doFoundFromKeyward = false;
    auto endPos = foreachArrayElement(line, 0, [&](auto line) {
        auto [indexStart, indexEnd] = line.getRangeSeparatedBySpace(0);

        auto indexStr = line.substr(indexStart, indexEnd - indexStart);
        auto index = parseArrayIndex(indexStr);
        arrayAccessorScope.pushIndex(index);

        auto [keywardStart, keywardEnd] = line.getRangeSeparatedBySpace(indexEnd);
        auto keyward = line.substr(keywardStart, keywardEnd - keywardStart);
        if (keyward == "from") {
            doFoundFromKeyward = true;
            return keywardEnd;
        }
        return GO_NEXT_ELEMENT;
    });

    if (doFoundFromKeyward) {
        bool doFound = false;
        auto [nestName, nameEnd] = parseName(line, endPos, doFound);
        if (!doFound) {
            AWESOME_THROW(ArrayAccessException) << "Unfound access target...";
        }
        auto pValue = env.searchValue(toStringList(nestName), false);
        switch (pValue->type) {
        case Value::Type::Function:
        case Value::Type::Coroutine:
            env.pushScope(std::make_shared<CallFunctionScope>(env.currentScope(), *pValue));
            env.pushMode(std::make_shared<CallFunctionParseMode>());
            return env.currentMode()->parse(env, Line(line, nameEnd));

        case Value::Type::Array:
            arrayAccessorScope.setValueToPass(pValue->get<Value::array>());
            break;

        default:
            AWESOME_THROW(SyntaxException) << "An attempt was made to access a value other than Array or Function...";
        }
    }
    return Result::Continue;
}

}