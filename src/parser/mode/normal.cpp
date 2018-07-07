#include "normal.h"

#include <iostream>

#include <boost/optional.hpp>

#include "../parserUtility.h"
#include "../line.h"
#include "../enviroment.h"

#include "multiLineComment.h"
#include "objectDefined.h"

using namespace std;

namespace parser
{

IParseMode::Result NormalParseMode::parse(Enviroment& env, Line& line)
{
    auto commentType = evalComment(env, line);
    if (CommentType::MultiLine == commentType) {
        return Result::Next;
    }

    int level = evalIndent(env, line);
    if (line.length() <= 0) {
        return Result::Next;
    }

    int resultCompared = env.compareIndentLevel(level);
    if ( 0 < resultCompared) {
        throw MakeException<SyntaxException>()
            << "An indent above current scope depth is described." << MAKE_EXCEPTION;
    }

    if (resultCompared < 0) {
        while (0 != env.compareIndentLevel(level)) {
            // close top scope.
            closeTopScope(env);
        }
    }

    if (Value::Type::Object == env.currentScope().valueType()) {
        auto [nestNames, p] = parseName(line, 0);
        if (nestNames.empty()) {
            throw MakeException<SyntaxException>()
                << "found invalid character." << MAKE_EXCEPTION;
        }

        auto opType = parseOperator(p, line, p);
        if (OperatorType::Unknown == opType) {
            throw MakeException<SyntaxException>()
                << "found unknown operater." << MAKE_EXCEPTION;
        }

        // parse value
        p = line.skipSpace(p);
        if (OperatorType::Are == opType) {
            env.pushScope(std::make_shared<NormalScope>(nestNames, Value().init(Value::Type::Array)));
            p = parseArrayElement(env, line, p);
        } else if (OperatorType::Copy == opType) {
            env.pushScope(std::make_shared<NormalScope>(nestNames, Value().init(Value::Type::None)));
            auto [srcNestNameView, endPos] = parseName(line, p);
            if (srcNestNameView.empty()) {
                throw MakeException<SyntaxException>()
                    << "found invalid character in source variable." << MAKE_EXCEPTION;
            }
            std::list<std::string> srcNestName = toStringList(srcNestNameView);
            Value* pValue = searchValue(srcNestName, env);
            env.currentScope().value() = *pValue;
        } else if (OperatorType::PushBack == opType) {
            // push reference scope
            std::list<std::string> targetNestName = toStringList(nestNames);
            Value* pValue = searchValue(targetNestName, env);
            if (Value::Type::Array != pValue->type) {
                throw MakeException<SyntaxException>() << "An attempt was made to add with a value other than an array."
                    << MAKE_EXCEPTION;
            }
            env.pushScope(std::make_shared<ReferenceScope>(targetNestName, *pValue));

            // push value scope of anonymous
            env.pushScope(std::make_shared<NormalScope>(std::list<std::string>{""}, Value().init(Value::Type::None)));
            auto valueLine = Line(line.get(p), 0, line.length() - p);
            parseValue(env, valueLine);

        } else if (OperatorType::Remove == opType) {
            //TODO?

        } else if(OperatorType::Extend == opType) {
            auto objectNameLine = Line(line.get(p), 0, line.length() - p);
            auto[objectNestName, endPos] = parseObjectName(env, line, p);

            //TODO seach Objectdefined from env and set it to new scope 
            ObjectDefined const* pObjectDefined = searchObjdectDefined(objectNestName, env);

            Value objDefiend;
            objDefiend = *pObjectDefined;
            env.pushScope(std::make_shared<NormalScope>(nestNames, std::move(objDefiend)));
            env.pushMode(std::make_shared<ObjectDefinedParseMode>());

        } else {
            env.pushScope(std::make_shared<NormalScope>(nestNames, Value().init(Value::Type::None)));
            auto valueLine = Line(line.get(p), 0, line.length() - p);
            parseValue(env, valueLine);
        }

        //cout << env.source.row() << "," << env.indent.currentLevel() << "," << env.scopeStack.size() << ":"
        //    << " name=";
        //for (auto&& n : nestNames) {
        //    cout << n << ".";
        //}
        //cout << " op=" << toString(opType) << ": scopeLevel=" << env.scopeStack.size() << endl;
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
        cout << env.source.row() << "," << env.indent.currentLevel() << "," << env.scopeStack.size() << ":"
            << Value::toString(env.currentScope().valueType()) << endl;
    }
    return Result::Next;
}

}