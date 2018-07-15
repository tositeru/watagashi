#include "branch.h"

#include "../enviroment.h"
#include "../line.h"

namespace parser
{

IParseMode::Result BranchParseMode::parse(Enviroment& env, Line& line)
{
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

                //check denial
                auto[isDenial, dinialEndPos] = doExistDenialKeyward(line);
                if (isDenial) {
                    line.resize(dinialEndPos, 0);
                }

                //
                auto[boolVarNestName, nameEndPos] = parseName(line, 0);
                Value const* pValue = searchValue(toStringList(boolVarNestName), env);
                if (Value::Type::Bool != pValue->type) {
                    AWESOME_THROW(BooleanException) << "A value other than bool type can not be described in BooleanScope by itself.";
                }
                auto b = pValue->get<bool>();
                branchScope.tally(isDenial ? !b : b);
            } else {
                // compare values.
                Value const* pLeftValue;
                if (branchScope.isSwitch()) {
                    pLeftValue = &branchScope.switchTargetValue();
                } else {
                    auto leftValueLine = Line(line.get(0), 0, compareOpStart - 1);
                    Value leftValue;
                    pLeftValue = getValue(leftValue, env, leftValueLine);
                }

                auto startRightValue = line.incrementPos(compareOpStart, [](auto line, auto pos) {
                    return !isSpace(line.get(pos));
                });
                startRightValue = line.skipSpace(startRightValue);

                auto rightValueLine = Line(line.get(startRightValue), 0, line.length() - startRightValue);
                Value rightValue;
                Value const* pRightValue = getValue(rightValue, env, rightValueLine);

                bool isDenial = false;
                bool result = compareValues(*pLeftValue, *pRightValue, compareOp);
                branchScope.tally(isDenial ? !result : result);
            }

            return true;
        });
    }

    return Result::Continue;
}


}