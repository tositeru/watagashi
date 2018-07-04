#include "objectDefined.h"

#include <iostream>

#include "../line.h"
#include "../enviroment.h"

using namespace std;

namespace parser
{

IParseMode::Result ObjectDefinedParseMode::parse(Enviroment& env, Line& line)
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
    if (0 < resultCompared) {
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
        if (env.currentMode().get() != this) {
            return Result::Redo;
        }
    }

    if (Value::Type::ObjectDefined == env.currentScope().valueType()) {
        size_t p = 0;
        auto nestNames = parseName(p, line, 0);
        if (nestNames.empty()) {
            cerr << env.source.row() << ": syntax error!! found invalid character.\n"
                << line.string_view() << endl;
            return Result::Next;
        }

        if (2 <= nestNames.size()) {
            cerr << env.source.row() << ": syntax error!! Don't write member of ObjectDefined by nest name.\n"
                << line.string_view() << endl;
            return Result::Next;
        }

        auto opType = parseOperator(p, line, p);
        if (OperatorType::Is != opType) {
            cerr << env.source.row() << ": syntax error!! found unknown operater.\n"
                << line.string_view() << endl;
            return Result::Next;
        }

        auto valueType = parseValueType(env, line, p);
        if (Value::Type::None == valueType
            || Value::Type::ObjectDefined == valueType) {
            cerr << env.source.row() << ": syntax error!! found unknown value type.\n"
                << line.string_view() << endl;
            return Result::Next;
        }
        //cout << env.source.row() << ": " << nestNames.back() << "=" << Value::toString(valueType) << endl;

        MemberDefined memberDefined;
        memberDefined.type = valueType;
        Value memberDefiend;
        memberDefiend = std::move(memberDefined);
        env.pushScope(std::make_shared<NormalScope>(std::move(nestNames), std::move(memberDefiend)));

        p = line.skipSpace(p);
        if (line.isEndLine(p)) {
            return Result::Next;
        }

        // parse default value
        auto memberDefinedOp = parseMemberDefinedOperator(p, line, p);
        if (MemberDefinedOperatorType::Unknown == memberDefinedOp) {
            cerr << env.source.row() << ": syntax error!! found unknown MemberDefined operator.\n"
                << line.string_view() << endl;
            return Result::Next;
        }
        p = line.skipSpace(p);
        if (Value::Type::Array == valueType) {
            env.pushScope(std::make_shared<NormalScope>(std::list<std::string>{""}, Value().init(Value::Type::Array)));
        } else {
            auto initValue = Value().init(valueType);
            env.pushScope(std::make_shared<NormalScope>(std::list<std::string>{""}, std::move(initValue)));
        }
        if (line.isEndLine(p)) {
            return Result::Next;
        }

        if (Value::Type::Array == valueType) {
            p = parseArrayElement(env, line, p);
        } else {
            auto valueLine = Line(line.get(p), 0, line.length()-p);
            if (auto error = parseValue(env, valueLine)) {
                cerr << error.message()
                    << line.string_view() << endl;
                return Result::Next;
            }
        }


    } else if (Value::Type::MemberDefined == env.currentScope().valueType()) {

    } else if (Value::Type::Array == env.currentScope().valueType()) {
        auto p = parseArrayElement(env, line, 0);

    } else if (Value::Type::String == env.currentScope().valueType()) {
        env.currentScope().value().appendStr(line.string_view());

    } else if (Value::Type::Number == env.currentScope().valueType()) {
        // Convert Number to String when scope is multiple line.
        auto str = Value().init(Value::Type::String);
        str = std::to_string(env.currentScope().value().get<Value::number>());
        env.currentScope().value() = str;
    }

    return Result::Next;
}

}