#include "objectDefined.h"

#include <iostream>

#include "../line.h"
#include "../enviroment.h"

using namespace std;

namespace parser
{

IParseMode::Result ObjectDefinedParseMode::parse(Enviroment& env, Line& line)
{
    if (Value::Type::ObjectDefined == env.currentScope().valueType()) {
        auto [nestNames, p] = parseName(line, 0);
        if (nestNames.empty()) {
            throw MakeException<SyntaxException>()
                << "found invalid character." << MAKE_EXCEPTION;
        }
        if (2 <= nestNames.size()) {
            throw MakeException<SyntaxException>()
                << "Don't write member of ObjectDefined by nest name." << MAKE_EXCEPTION;
        }

        auto opType = parseOperator(p, line, p);
        if (OperatorType::Is != opType) {
            throw MakeException<SyntaxException>()
                << "found unknown operater." << MAKE_EXCEPTION;
        }

        auto valueType = parseValueType(line, p);
        if (Value::Type::None == valueType
            || Value::Type::ObjectDefined == valueType) {
            throw MakeException<SyntaxException>()
                << "found unknown value type." << MAKE_EXCEPTION;
        }

        MemberDefined tmp;
        tmp.type = valueType;
        Value memberDefined;
        memberDefined = std::move(tmp);
        env.pushScope(std::make_shared<NormalScope>(std::move(nestNames), std::move(memberDefined)));
        p = line.skipSpace(p);
        if (line.isEndLine(p)) {
            return Result::Continue;
        }

        // parse default value
        auto memberDefinedOp = parseMemberDefinedOperator(p, line, p);
        if (MemberDefinedOperatorType::Unknown == memberDefinedOp) {
            throw MakeException<SyntaxException>()
                << "found unknown MemberDefined operator." << MAKE_EXCEPTION;
        }
        p = line.skipSpace(p);
        env.pushScope(std::make_shared<NormalScope>(std::list<std::string>{""}, Value().init(valueType)));
        if (line.isEndLine(p)) {
            return Result::Continue;
        }

        if (Value::Type::Array == valueType) {
            p = parseArrayElement(env, line, p);
        } else {
            auto valueLine = Line(line.get(p), 0, line.length()-p);
            parseValue(env, valueLine);
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

    return Result::Continue;
}

}