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

    int level;
    if (auto error = evalIndent(env, line, level)) {
        cerr << error.message() << endl;
        return Result::Next;
    }

    if (line.length() <= 0) {
        return Result::Next;
    }

    int resultCompared = env.compareIndentLevel(level);
    if ( 0 < resultCompared) {
        cerr << env.source.row() << ": syntax error!! An indent above current scope depth is described.\n"
            << line.string_view() << endl;
        return Result::Next;
    }

    if (resultCompared < 0) {
        while (0 != env.compareIndentLevel(level)) {
            // close top scope.
            if (auto error = closeTopScope(env)) {
                cerr << error.message() << "\n"
                    << line.string_view() << endl;
            }
        }
    }

    if (Value::Type::Object == env.currentScope().valueType()) {
        size_t p = 0;
        auto nestNames = parseName(p, line, 0);
        if (nestNames.empty()) {
            cerr << env.source.row() << ": syntax error!! found invalid character.\n"
                << line.string_view() << endl;
            return Result::Next;
        }

        auto opType = parseOperator(p, line, p);
        if (OperatorType::Unknown == opType) {
            cerr << env.source.row() << ": syntax error!! found unknown operater.\n"
                << line.string_view() << endl;
            return Result::Next;
        }

        // parse value
        p = line.skipSpace(p);
        if (OperatorType::Are == opType) {
            env.pushScope(std::make_shared<NormalScope>(nestNames, Value().init(Value::Type::Array)));
            p = parseArrayElement(env, line, p);
        } else if (OperatorType::Copy == opType) {
            env.pushScope(std::make_shared<NormalScope>(nestNames, Value().init(Value::Type::None)));
            decltype(p) tail = 0;
            auto srcNestNameView = parseName(tail, line, p);
            if (srcNestNameView.empty()) {
                cerr << env.source.row() << ": syntax error!! found invalid character in source variable.\n"
                    << line.string_view() << endl;
                return Result::Next;
            }
            std::list<std::string> srcNestName = toStringList(srcNestNameView);
            Value* pValue = nullptr;
            if (auto error = searchValue(&pValue, srcNestName, env)) {
                cerr << error.message()
                    << line.string_view() << endl;
                return Result::Next;
            }
            env.currentScope().value() = *pValue;

        } else if (OperatorType::PushBack == opType) {
            // push reference scope
            std::list<std::string> targetNestName = toStringList(nestNames);
            Value* pValue = nullptr;
            if (auto error = searchValue(&pValue, targetNestName, env)) {
                cerr << error.message()
                    << "syntax error!! Don't found the push_back target array.\n"
                    << line.string_view() << endl;
                return Result::Next;
            }
            if (Value::Type::Array != pValue->type) {
                ErrorHandle error = MakeErrorHandle(env.source.row()) << "syntax error!! An attempt was made to add with a value other than an array.";
                cerr << error.message() << endl;
                return Result::Next;
            }
            env.pushScope(std::make_shared<ReferenceScope>(targetNestName, *pValue));

            // push value scope of anonymous
            env.pushScope(std::make_shared<NormalScope>(std::list<std::string>{""}, Value().init(Value::Type::None)));
            auto valueLine = Line(line.get(p), 0, line.length() - p);
            if (auto error = parseValue(env, valueLine)) {
                cerr << error.message()
                    << line.string_view() << endl;
                env.popScope();
                return Result::Next;
            }

        } else if (OperatorType::Remove == opType) {
            //TODO?

        } else if(OperatorType::Extend == opType) {
            auto objectNameLine = Line(line.get(p), 0, line.length() - p);
            boost::string_view rawObjectName;
            if (auto error = parseObjectName(rawObjectName, p, env, objectNameLine, 0)) {
                cerr << error.message()
                    << line.string_view() << endl;
                return Result::Next;
            }
            env.pushScope(std::make_shared<NormalScope>(nestNames, Value().init(Value::Type::ObjectDefined)));
            env.pushMode(std::make_shared<ObjectDefinedParseMode>());

        } else {
            env.pushScope(std::make_shared<NormalScope>(nestNames, Value().init(Value::Type::None)));
            auto valueLine = Line(line.get(p), 0, line.length() - p);
            if (auto error = parseValue(env, valueLine)) {
                cerr << error.message()
                    << line.string_view() << endl;
                env.popScope();
                return Result::Next;
            }
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
        str.data = std::to_string(env.currentScope().value().get<Value::number>());
        env.currentScope().value() = str;

    } else {
        cout << env.source.row() << "," << env.indent.currentLevel() << "," << env.scopeStack.size() << ":"
            << Value::toString(env.currentScope().valueType()) << endl;
    }
    return Result::Next;
}

}