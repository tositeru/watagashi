#include "boolean.h"

#include <iostream>

#include "../line.h"
#include "../enviroment.h"

using namespace std;

namespace parser
{

std::tuple<LogicOperator, size_t> findLogicOperator(Line const& line, size_t start)
{
    auto const isNotSeparator = [](auto line, auto pos) {
        return !isSpace(line.get(pos)) && ',' != *line.get(pos);
    };

    auto pos = line.skipSpace(start);
    // search logical operator beginning with '\'
    while (!line.isEndLine(pos)) {
        auto backSlashPos = line.incrementPos(pos, [](auto line, auto pos) {
            return '\\' != *line.get(pos);
        });
        //if (backSlashPos == line.length()) {
        //    break;
        //}
        auto wordEnd = line.incrementPos(backSlashPos, isNotSeparator);
        if (wordEnd == backSlashPos) {
            wordEnd = std::min(wordEnd+1, line.length());
        }
        auto keyward = line.substr(backSlashPos+1, wordEnd - backSlashPos);
        auto logicOp = toLogicOperatorType(keyward);
        if (LogicOperator::Unknown != logicOp) {
            return { logicOp, wordEnd };
        }
        pos = wordEnd;
        if (',' != *line.get(wordEnd))
            ++pos;
    }

    // search logical operator.
    pos = line.skipSpace(start);
    while (!line.isEndLine(pos)) {
        auto wordEnd = line.incrementPos(pos, isNotSeparator);
        //if (line.length() <= wordEnd) {
        //    pos = line.length();
        //    break;
        //}
        if (wordEnd == pos) {
            wordEnd = std::min(wordEnd + 1, line.length());
        }
        auto keyward = line.substr(pos, wordEnd-pos);
        auto logicOp = toLogicOperatorType(keyward);
        if (LogicOperator::Unknown != logicOp) {
            return { logicOp, wordEnd };
        }
        pos = wordEnd;
        if (',' != *line.get(wordEnd))
            ++pos;
    }
    return {LogicOperator::Continue, std::min(pos, line.length()) };
}

std::tuple<CompareOperator, size_t> findCompareOperator(Line const& line, size_t start)
{
    auto pos = line.skipSpace(start);
    // search logical operator beginning with '\'
    while (!line.isEndLine(pos)) {
        auto backSlashPos = line.incrementPos(pos, [](auto line, auto pos) {
            return '\\' != *line.get(pos);
        });
        if (backSlashPos == line.length()) {
            break;
        }
        auto wordEnd = line.incrementPos(backSlashPos, [](auto line, auto pos) {
            return !isSpace(line.get(pos));
        });
        auto keyward = line.substr(backSlashPos + 1, wordEnd - backSlashPos);
        auto compareOp = toCompareOperatorType(keyward);
        if (CompareOperator::Unknown != compareOp) {
            return { compareOp, wordEnd };
        }
        pos = wordEnd + 1;
    }

    // search logical operator.
    pos = line.skipSpace(start);
    while (!line.isEndLine(pos)) {
        auto wordEnd = line.incrementPos(pos, [](auto line, auto pos) {
            return !isSpace(line.get(pos));
        });
        if (wordEnd == line.length()) {
            break;
        }
        auto keyward = line.substr(pos, wordEnd-pos);
        auto compareOp = toCompareOperatorType(keyward);
        if (CompareOperator::Unknown != compareOp) {
            return { compareOp, wordEnd };
        }
        pos = wordEnd + 1;
    }
    return { CompareOperator::Unknown, std::min(pos, line.length()) };
}

Value const* getValue(Value& outValueEntity, Enviroment const& env, Line& valueLine)
{
    bool isSuccess = false;
    auto[nestNameView, leftValueNamePos] = parseName(valueLine, 0, isSuccess);
    Value const* pValue = nullptr;
    if (isSuccess) {
        pValue = searchValue(isSuccess, toStringList(nestNameView), env);
    }
    if (!pValue) {
        outValueEntity = parseValueInSingleLine(env, valueLine);
        pValue = &outValueEntity;
    }
    return pValue;
}

IParseMode::Result BooleanParseMode::parse(Enviroment& env, Line& line)
{
    if (line.length() <= 0) {
        return Result::Continue;
    }

    if (IScope::Type::Boolean == env.currentScope().type()) {
        auto& booleanScope = dynamic_cast<BooleanScope&>(env.currentScope());

        auto pos = 0;
        while (!line.isEndLine(pos)) {
            auto start = pos;
            auto [logicOp, logicOpEnd] = findLogicOperator(line, start);
            if (LogicOperator::Unknown != logicOp) {
                booleanScope.setLogicOperator(logicOp);
            }
            if (!booleanScope.doEvalValue()) {
                pos = logicOpEnd;
                continue;
            }

            auto booleanLine = Line(line.get(start), 0, logicOpEnd - start);
            auto [compareOp, compareOpStart] = findCompareOperator(booleanLine, 0);
            if (CompareOperator::Unknown == compareOp) {
                // eval var of bool type

                //check denial
                bool isDenial = false;
                if (5 <= booleanLine.length()) {
                    if ("not " == booleanLine.substr(0, 4)) {
                        isDenial = true;

                        booleanLine.resize(4, 0);
                        auto p = booleanLine.skipSpace(0);
                        booleanLine.resize(p, 0);
                    }
                }
                if (3 <= booleanLine.length()) {
                    if ("!" == booleanLine.substr(0, 1)) {
                        isDenial = true;

                        booleanLine.resize(1, 0);
                        auto p = booleanLine.skipSpace(0);
                        booleanLine.resize(p, 0);
                    }
                }

                //
                auto [boolVarNestName, nameEndPos] = parseName(booleanLine, 0);
                Value const* pValue = searchValue(toStringList(boolVarNestName), env);

                if (Value::Type::Bool != pValue->type) {
                    AWESOME_THROW(BooleanException) << "A value other than bool type can not be described in BooleanScope by itself.";
                }
                auto b = pValue->get<bool>();
                booleanScope.tally(isDenial ? !b : b);
                pos = logicOpEnd;
                continue;
            }

            // compare values.
            cout << "compare value:";
            auto leftValueLine = Line(line.get(0), 0, compareOpStart-1);
            Value leftValue;
            Value const* pLeftValue = getValue(leftValue, env, leftValueLine);

            auto startRightValue = booleanLine.incrementPos(compareOpStart, [](auto line, auto pos) {
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
            bool isResult = false;
            switch (compareOp) {
            case CompareOperator::Equal: isResult = (*pLeftValue == *pRightValue); break;
            case CompareOperator::NotEqual: isResult = (*pLeftValue != *pRightValue); break;
            case CompareOperator::Greater: isResult = (*pLeftValue > *pRightValue); break;
            case CompareOperator::GreaterEqual: isResult = (*pLeftValue >= *pRightValue); break;
            case CompareOperator::Less: isResult = (*pLeftValue < *pRightValue); break;
            case CompareOperator::LessEqual: isResult = (*pLeftValue <= *pRightValue); break;
            default:
                AWESOME_THROW(BooleanException) << "use unknown compare...";
            }
            booleanScope.tally(isResult);

            cout << endl;

            pos = logicOpEnd;
        }
    }

    return Result::Continue;
}

}