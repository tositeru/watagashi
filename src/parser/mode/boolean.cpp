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
        auto& booleanScope = dynamic_cast<BooleanScope&>(env.currentScope());

        foreachLogicOperator(line, 0, [&](auto line, auto logicOp) {
            if (LogicOperator::Unknown != logicOp) {
                booleanScope.setLogicOperator(logicOp);
            }
            if (!booleanScope.doEvalValue()) {
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
                booleanScope.tally(isDenial ? !b : b);

            } else {
                // compare values.
                auto leftValueLine = Line(line.get(0), 0, compareOpStart - 1);
                Value leftValue;
                Value const* pLeftValue = getValue(leftValue, env, leftValueLine);

                auto startRightValue = line.incrementPos(compareOpStart, [](auto line, auto pos) {
                    return !isSpace(line.get(pos));
                });
                auto rightValueLine = Line(line.get(startRightValue), 0, line.length() - startRightValue);
                Value rightValue;
                Value const* pRightValue = getValue(rightValue, env, rightValueLine);

                if (pLeftValue->type != pRightValue->type) {
                    AWESOME_THROW(BooleanException)
                        << "Values of diffrernt types can not compare... "
                        << "left=" << Value::toString(pLeftValue->type)
                        << ", right=" << Value::toString(pRightValue->type);
                }
                bool isResult = compareValues(*pLeftValue, *pRightValue, compareOp);
                booleanScope.tally(isResult);
            }
            return true;
        });

        //auto pos = 0;
        //while (!line.isEndLine(pos)) {
        //    auto start = pos;
        //    auto [logicOp, logicOpEnd] = findLogicOperator(line, start);
        //    if (LogicOperator::Unknown != logicOp) {
        //        booleanScope.setLogicOperator(logicOp);
        //    }
        //    if (!booleanScope.doEvalValue()) {
        //        pos = logicOpEnd;
        //        continue;
        //    }

        //    auto booleanLine = Line(line.get(start), 0, logicOpEnd - start);
        //    auto [compareOp, compareOpStart] = findCompareOperator(booleanLine, 0);
        //    if (CompareOperator::Unknown == compareOp) {
        //        // eval var of bool type

        //        //check denial
        //        bool isDenial = false;
        //        if (5 <= booleanLine.length()) {
        //            if ("not " == booleanLine.substr(0, 4)) {
        //                isDenial = true;

        //                booleanLine.resize(4, 0);
        //                auto p = booleanLine.skipSpace(0);
        //                booleanLine.resize(p, 0);
        //            }
        //        }
        //        if (3 <= booleanLine.length()) {
        //            if ("!" == booleanLine.substr(0, 1)) {
        //                isDenial = true;

        //                booleanLine.resize(1, 0);
        //                auto p = booleanLine.skipSpace(0);
        //                booleanLine.resize(p, 0);
        //            }
        //        }

        //        //
        //        auto [boolVarNestName, nameEndPos] = parseName(booleanLine, 0);
        //        Value const* pValue = searchValue(toStringList(boolVarNestName), env);

        //        if (Value::Type::Bool != pValue->type) {
        //            AWESOME_THROW(BooleanException) << "A value other than bool type can not be described in BooleanScope by itself.";
        //        }
        //        auto b = pValue->get<bool>();
        //        booleanScope.tally(isDenial ? !b : b);
        //        pos = logicOpEnd;

        //    } else {
        //        // compare values.
        //        auto leftValueLine = Line(line.get(0), 0, compareOpStart-1);
        //        Value leftValue;
        //        Value const* pLeftValue = getValue(leftValue, env, leftValueLine);

        //        auto startRightValue = booleanLine.incrementPos(compareOpStart, [](auto line, auto pos) {
        //            return !isSpace(line.get(pos));
        //        });
        //        auto rightValueLine = Line(line.get(startRightValue), 0, line.length() - startRightValue);
        //        Value rightValue;
        //        Value const* pRightValue = getValue(rightValue, env, rightValueLine);
 
        //        if (pLeftValue->type != pRightValue->type) {
        //            AWESOME_THROW(BooleanException)
        //                << "Values of diffrernt types can not compare... "
        //                << "left=" << Value::toString(pLeftValue->type)
        //                << ", right=" << Value::toString(pRightValue->type);
        //        }
        //        bool isResult = compareValues(*pLeftValue, *pRightValue, compareOp);
        //        booleanScope.tally(isResult);

        //        pos = logicOpEnd;
        //    }

        //}
    }

    return Result::Continue;
}

}