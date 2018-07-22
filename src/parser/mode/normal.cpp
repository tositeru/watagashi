#include "normal.h"

#include <iostream>

#include <boost/optional.hpp>

#include "../parserUtility.h"
#include "../line.h"
#include "../enviroment.h"

#include "multiLineComment.h"
#include "objectDefined.h"
#include "boolean.h"
#include "branch.h"
#include "defineFunction.h"
#include "callFunction.h"
#include "send.h"
#include "arrayAccessor.h"

using namespace std;

namespace parser
{

IParseMode::Result parseMember(Enviroment& env, Line& line)
{
    auto[nestNames, p] = parseName(line, 0);
    if (nestNames.empty()) {
        AWESOME_THROW(SyntaxException) << "found invalid character.";
    }

    auto opType = parseOperator(p, line, p);
    if (OperatorType::Unknown == opType) {
        AWESOME_THROW(SyntaxException) << "found unknown operater.";
    }

    // parse value
    p = line.skipSpace(p);
    if (OperatorType::Is == opType) {
        env.pushScope(std::make_shared<NormalScope>(nestNames, Value().init(Value::Type::None)));
        auto valueLine = Line(line.get(p), 0, line.length() - p);
        parseValue(env, valueLine);

    } else if (OperatorType::Are == opType) {
        env.pushScope(std::make_shared<NormalScope>(nestNames, Value().init(Value::Type::Array)));
        p = parseArrayElement(env, line, p);

    } else if (OperatorType::Judge == opType || OperatorType::Deny == opType) {
        //skip member name and operator in the line so that BooleanParseMode does not parse them.
        line.resize(p, 0);
        env.pushScope(std::make_shared<BooleanScope>(nestNames, OperatorType::Deny == opType));
        env.pushMode(std::make_shared<BooleanParseMode>());
        auto pMode = env.currentMode();
        return pMode->parse(env, line);

    } else if (OperatorType::Copy == opType) {
        env.pushScope(std::make_shared<NormalScope>(nestNames, Value().init(Value::Type::None)));
        auto[srcNestNameView, endPos] = parseName(line, p);
        if (srcNestNameView.empty()) {
            AWESOME_THROW(SyntaxException)
                << "found invalid character in source variable.";
        }
        std::list<std::string> srcNestName = toStringList(srcNestNameView);
        Value const* pValue = env.searchValue(srcNestName, false);
        env.currentScope().value() = *pValue;

    } else if (OperatorType::Receive == opType) {
        env.pushScope(std::make_shared<ArrayAccessorScope>(nestNames));
        auto arrayAccessorLine = Line(line, line.skipSpace(p));
        env.pushMode(std::make_shared<ArrayAccessorParseMode>());
        return env.currentMode()->parse(env, arrayAccessorLine);
    } else if (OperatorType::PushBack == opType) {
        // push reference scope
        std::list<std::string> targetNestName = toStringList(nestNames);
        Value* pValue = env.searchValue(targetNestName, false);
        if (Value::Type::Array != pValue->type) {
            AWESOME_THROW(SyntaxException) << "An attempt was made to add with a value other than an array.";
        }
        env.pushScope(std::make_shared<ReferenceScope>(targetNestName, *pValue, false));

        // push value scope of anonymous
        env.pushScope(std::make_shared<NormalScope>(std::list<std::string>{""}, Value().init(Value::Type::None)));
        auto valueLine = Line(line.get(p), 0, line.length() - p);
        parseValue(env, valueLine);

    } else if (OperatorType::Remove == opType) {
        //TODO?

    } else if (OperatorType::DefineFunction == opType) {
        line.resize(p, 0);
        env.pushScope(std::make_shared<NormalScope>(nestNames, Value().init(Value::Type::Function)));
        env.pushMode(std::make_shared<DefineFunctionParseMode>());
        auto pMode = env.currentMode();
        return pMode->parse(env, line);

    } else if (OperatorType::Extend == opType) {
        auto objectNameLine = Line(line.get(p), 0, line.length() - p);
        auto[objectNestName, endPos] = parseObjectName(line, p);

        ObjectDefined const* pObjectDefined = env.searchObjdectDefined(objectNestName);

        Value objDefiend;
        objDefiend = *pObjectDefined;
        env.pushScope(std::make_shared<NormalScope>(nestNames, std::move(objDefiend)));
        env.pushMode(std::make_shared<ObjectDefinedParseMode>());

    } else {
        AWESOME_THROW(SyntaxException) << "Unknown operator type...";
    }

    return IParseMode::Result::Continue;
}

IParseMode::Result parseStatement(Enviroment& env, Line& line)
{
    auto statementEnd = line.incrementPos(0, [](auto line, auto pos) {
        return !isSpace(line.get(pos));
    });
    auto statement = toStatementType(line.substr(0, statementEnd));
    switch (statement) {
    case Statement::Branch: [[fallthrough]];
    case Statement::Unless:
    {
        auto pos = line.skipSpace(statementEnd);
        bool isSuccess = false;
        auto [nestNameView, nameEnd] = parseName(line, pos, isSuccess);
        Value const* pValue = nullptr;
        if (isSuccess) {
            pValue = env.searchValue(toStringList(nestNameView), false);
        }
        env.pushScope(std::make_shared<BranchScope>(env.currentScope(), pValue, Statement::Unless == statement));
        env.pushMode(std::make_shared<BranchParseMode>());
        break;
    }
    case Statement::EmptyLine:
        return IParseMode::Result::NextLine;
    case Statement::Send:
        env.pushScope(std::make_shared<SendScope>());
        env.pushMode(std::make_shared<SendParseMode>());

        return env.currentMode()->parse(env, Line(line, line.skipSpace(statementEnd)));

    default:
    {
        auto[nestNames, p] = parseName(line, 0);
        auto pFunc = env.searchValue(toStringList(nestNames), false, nullptr);
        if (!pFunc || pFunc->type != Value::Type::Function) {
            AWESOME_THROW(SyntaxException) << "Don't found function... name='" << toNameString(nestNames) << "'";
        }

        env.pushScope(std::make_shared<CallFunctionScope>(env.currentScope(), pFunc->get<Function>()));
        env.pushMode(std::make_shared<CallFunctionParseMode>());
        return env.currentMode()->parse(env, Line(line, line.skipSpace(p)));
        break;
    }
    }
    return IParseMode::Result::Continue;
}

IParseMode::Result NormalParseMode::parse(Enviroment& env, Line& line)
{
    line.resize(line.skipSpace(0), 0);
    if (Value::Type::Object == env.currentScope().valueType()) {
        if (':' == *line.get(0)) {
            auto statementLine = line;
            statementLine.resize(1, 0);
            return parseStatement(env, statementLine);
        } else {
            return parseMember(env, line);
        }

    } else if (Value::Type::Array == env.currentScope().valueType()) {
        auto p = parseArrayElement(env, line, 0);

    } else if (Value::Type::String == env.currentScope().valueType()) {
        env.currentScope().value().appendStr(line.string_view());

    } else if (Value::Type::Number == env.currentScope().valueType()) {
        // Convert Number to String when scope is multiple line.
        auto str = Value().init(Value::Type::String);
        str = std::to_string(env.currentScope().value().get<Value::number>());
        env.currentScope().value() = str;

    } else {
        AWESOME_THROW(FatalException) << "unimplement...";
    }
    return Result::Continue;
}

}