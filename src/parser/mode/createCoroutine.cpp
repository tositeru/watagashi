#include "createCoroutine.h"

#include "../line.h"
#include "../enviroment.h"

#include "normal.h"

namespace parser
{

IParseMode::Result CreateCoroutineParseMode::parse(Enviroment& env, Line& line)
{
    if (line.length() <= 0) {
        return Result::Continue;
    }

    switch (env.currentScope().type()) {
    case IScope::Type::Normal:                  return parseDefault(env, line);
    case IScope::Type::CallFunctionArguments:   return parseArguments(env, line);
    default:
        AWESOME_THROW(std::runtime_error) << "unknown parse mode...";
    }
    return Result::Continue;
}

IParseMode::Result CreateCoroutineParseMode::parseDefault(Enviroment& env, Line line)
{
    if (env.currentScope().valueType() != Value::Type::Coroutine) {
        AWESOME_THROW(FatalException) << "The scope of CreateCoroutineParseMode must have Coroutine value.";
    }
    auto& coroutine = env.currentScope().value().get<Value::coroutine>();

    auto[opStart, opEnd] = line.getRangeSeparatedBySpace(0);
    auto callOperator = toCallFunctionOperaotr(line.substr(opStart, opEnd));
    switch (callOperator) {
    case CallFunctionOperator::ByUsing:
        env.pushScope(std::make_shared<CallFunctionArgumentsScope>(env.currentScope(), coroutine.pFunction->arguments.size()));
        return this->parseArguments(env, Line(line, opEnd));
        break;
    default:
        AWESOME_THROW(SyntaxException) << "unknown operator...";
        break;
    }
    return Result::Continue;
}

IParseMode::Result CreateCoroutineParseMode::parseArguments(Enviroment& env, Line line)
{
    auto pCurrentScope = dynamic_cast<CallFunctionArgumentsScope*>(env.currentScopePointer().get());

    bool doSeekParseReturnValue = false;
    auto endPos = foreachArrayElement(line, 0, [&](auto elementLine) {
        while (pCurrentScope != env.currentScopePointer().get()) {
            env.closeTopScope();
        }

        bool isFoundValue = false;
        auto[nestName, nameEndPos] = parseName(elementLine, 0, isFoundValue);
        Value const* pValue = nullptr;
        if (isFoundValue) {
            pValue = env.searchValue(toStringList(nestName), false, nullptr);
        }

        if (pValue) {
            pCurrentScope->pushArgument(Value(*pValue));
        } else {
            env.pushScope(std::make_shared<NormalScope>(std::list<std::string>{""}, Value()));
            env.pushMode(std::make_shared<NormalParseMode>());
            parseValue(env, elementLine);
        }

        return GO_NEXT_ELEMENT;
    });

    return Result::Continue;
}

}