#include "branch.h"

#include "../enviroment.h"
#include "../line.h"

namespace parser
{

IParseMode::Result BranchParseMode::parse(Enviroment& env, Line& line)
{
    assert(IScope::Type::Branch == env.currentScope().type());

    auto& branchScope = dynamic_cast<BranchScope&>(env.currentScope());

    bool isElse = false;
    if (line.length() == 4) {
        auto str = line.substr(0, 4);
        if ("else" == str) {
            isElse = true;
        }
    }

    if (isElse) {
        branchScope.tally(branchScope.doElseStatement());

    } else {
        foreachLogicOperator(line, 0, [&](auto line, auto logicOp) {
            if (LogicOperator::Unknown != logicOp) {
                branchScope.setLogicOperator(logicOp);
            }
            if (!branchScope.doEvalValue()) {
                return false;
            }

            auto[compareOp, compareOpStart] = findCompareOperator(line, 0);
            if (CompareOperator::Unknown == compareOp) {
                // eval var of bool type
                auto[pBool, isDenial] = parseBool(env, line);
                auto b = pBool->get<bool>();
                branchScope.tally(isDenial ? !b : b);

            } else {
                // compare values.
                auto [leftValue, rightValue] = parseCompareTargetValues(
                    env, line, compareOpStart, branchScope.isSwitch() ? &branchScope.switchTargetValue() : nullptr, nullptr);
                bool result = compareValues(leftValue.value(), rightValue.value(), compareOp);
                branchScope.tally(result);
            }

            return true;
        });
    }

    return Result::Continue;
}


}