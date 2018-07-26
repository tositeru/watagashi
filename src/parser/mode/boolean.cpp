#include "boolean.h"

#include <iostream>

#include "../line.h"
#include "../enviroment.h"

using namespace std;

namespace parser
{

IParseMode::Result BooleanParseMode::parse(Enviroment& env, Line& line)
{
    if (line.length() <= 0) {
        return Result::Continue;
    }

    if (IScope::Type::Boolean == env.currentScope().type()) {
        assert(IScope::Type::Boolean == env.currentScope().type());
        auto& booleanScope = dynamic_cast<BooleanScope&>(env.currentScope());

        foreachLogicOperator(line, 0, [&](auto line, auto logicOp) {
            if (LogicOperator::Unknown != logicOp) {
                booleanScope.setLogicOperator(logicOp);
            }
            if (!booleanScope.doEvalValue()) {
                return false;
            }
            auto [compareOp, compareOpStart] = findCompareOperator(line, 0);
            if (CompareOperator::Unknown == compareOp) {
                // eval var of bool type
                auto [pBool, isDenial] = parseBool(env, line);
                auto b = pBool->get<bool>();
                booleanScope.tally(isDenial ? !b : b);

            } else {
                // compare values.
                auto[leftValue, rightValue] = parseCompareTargetValues(env, line, compareOpStart, nullptr, nullptr);
                bool result = compareValues(leftValue.value(), rightValue.value(), compareOp);
                booleanScope.tally(result);
            }
            return true;
        });
    }

    return Result::Continue;
}

}