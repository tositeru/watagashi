#include "callFunction.h"

#include "../line.h"
#include "../enviroment.h"

#include "normal.h"

namespace parser
{

IParseMode::Result CallFunctionParseMode::parse(Enviroment& env, Line& line)
{
    if (line.length() <= 0) {
        return Result::Continue;
    }
    
    switch (env.currentScope().type()) {
    case IScope::Type::CallFunction:                return parseDefault(env, line);
    case IScope::Type::CallFunctionArguments:       return parseArguments(env, line);
    case IScope::Type::CallFunctionReturnValues:    return parseReturnValues(env, line);
    default:
        AWESOME_THROW(std::runtime_error) << "unknown parse mode...";
    }
    return Result::Continue;
}

IParseMode::Result CallFunctionParseMode::parseDefault(Enviroment& env, Line line)
{
    auto& scope = dynamic_cast<CallFunctionScope&>(env.currentScope());
    auto[opStart, opEnd] = line.getRangeSeparatedBySpace(0);
    auto callOperator = toCallFunctionOperaotr(line.substr(opStart, opEnd));
    switch (callOperator) {
    case CallFunctionOperator::ByUsing:
        env.pushScope(std::make_shared<CallFunctionArgumentsScope>(scope, scope.function().arguments.size()));
        return this->parseArguments(env, Line(line, opEnd));
        break;
    case CallFunctionOperator::PassTo:
        env.pushScope(std::make_shared<CallFunctionReturnValueScope>(scope));
        return this->parseReturnValues(env, Line(line, opEnd));
    default:
        AWESOME_THROW(SyntaxException) << "unknown operator...";
        break;
    }
    return Result::Continue;
}

IParseMode::Result CallFunctionParseMode::parseArguments(Enviroment& env, Line line)
{
    auto pCurrentScope = dynamic_cast<CallFunctionArgumentsScope*>(env.currentScopePointer().get());

    bool doSeekParseReturnValue = false;
    auto endPos = foreachArrayElement(line, 0, [&](auto elementLine) {
        while (pCurrentScope != env.currentScopePointer().get()) {
            env.closeTopScope();
        }

        bool isFoundValue = false;
        auto [nestName, nameEndPos] = parseName(elementLine, 0, isFoundValue);
        Value const* pValue = nullptr;
        if (isFoundValue) {
            if (nestName.size() == 1) {
                if (nestName.front() == toString(CallFunctionOperator::PassTo)) {
                    doSeekParseReturnValue = true;
                    return size_t(0);
                }
            }

            pValue = env.searchValue(toStringList(nestName), false, nullptr);
        }

        if(pValue) {
            pCurrentScope->pushArgument(Value(*pValue));
        } else {
            env.pushScope(std::make_shared<NormalScope>(std::list<std::string>{""}, Value()));
            env.pushMode(std::make_shared<NormalParseMode>());
            parseValue(env, elementLine);
        }

        return GO_NEXT_ELEMENT;
    });

    if (doSeekParseReturnValue) {
        env.closeTopScope();
        auto pCallFunctionScope = dynamic_cast<CallFunctionScope*>(env.currentScopePointer().get());
        if (pCallFunctionScope) {
            env.pushScope(std::make_shared<CallFunctionReturnValueScope>(*pCallFunctionScope));
            auto returnValueLine = Line(line, endPos);
            auto [s, e] = returnValueLine.getRangeSeparatedBySpace(0);
            returnValueLine.resize(e, 0);
            return this->parseReturnValues(env, returnValueLine);
        }
    }

    return Result::Continue;
}

IParseMode::Result CallFunctionParseMode::parseReturnValues(Enviroment& env, Line line)
{
    auto pCurrentScope = dynamic_cast<CallFunctionReturnValueScope*>(env.currentScopePointer().get());
    auto endPos = foreachArrayElement(line, 0, [&](auto elementLine) {
        auto[nestName, nameEndPos] = parseName(elementLine, 0);
        pCurrentScope->pushReturnValueName(toStringList(nestName));
        return GO_NEXT_ELEMENT;
    });
    return Result::Continue;
}


}