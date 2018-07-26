#include "branch.h"

#include "../enviroment.h"
#include "../line.h"

#include "normal.h"

namespace parser
{

IParseMode::Result BranchParseMode::parse(Enviroment& env, Line& line)
{
    assert(IScope::Type::Branch == env.currentScope().type());

    auto& branchScope = dynamic_cast<BranchScope&>(env.currentScope());

    if(*line.get(0) == ':') {
        auto statementLine = line;
        statementLine.resize(1, 0);
        return parseStatement(env, statementLine);
    }

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

IParseMode::Result BranchParseMode::parseStatement(Enviroment& env, Line& line)
{
    auto statementEnd = line.incrementPos(0, [](auto line, auto pos) {
        return !isSpace(line.get(pos));
    });
    auto statement = toStatementType(line.substr(0, statementEnd));
    switch (statement) {
    case Statement::Local:
        env.pushMode(std::make_shared<NormalParseMode>());
        env.currentMode()->parse(env, Line(line, statementEnd));
        break;

    case Statement::EmptyLine:
        return IParseMode::Result::NextLine;

    default:
        AWESOME_THROW(SyntaxException) << "Found unknown statement...";
    }
    return IParseMode::Result::Continue;
}

}