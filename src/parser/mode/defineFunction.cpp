#include "defineFunction.h"

#include "../line.h"
#include "../enviroment.h"
#include "../scope.h"

#include "normal.h"

namespace parser
{

DefineFunctionParseMode::DefineFunctionParseMode()
    : mCurrentMode(Mode::Default)
{}

IParseMode::Result DefineFunctionParseMode::parse(Enviroment& env, Line& line)
{
    if (line.length() <= 0) {
        return Result::Continue;
    }

    switch (this->mCurrentMode) {
    case Mode::Default:         return parseByDefaultMode(env, line);
    case Mode::ToPass:          return parseByPassMode(env, line);
    case Mode::ToCapture:       return parseByCaptureMode(env, line);
    case Mode::WithContents:    return parseByContentsMode(env, line);
    default:
        AWESOME_THROW(std::runtime_error) << "unknown parse mode...";
        break;
    }
    return Result::Continue;
}

IParseMode::Result DefineFunctionParseMode::parseByDefaultMode(Enviroment& env, Line& line)
{
    auto [startKeyward, endKeyward] = line.getRangeSeparatedBySpace(0);
    auto keyward = line.substr(startKeyward, endKeyward);
    auto op = toDefineFunctionOperatorType(keyward);
    switch (op) {
    case DefineFunctionOperator::ToPass:    this->mCurrentMode = Mode::ToPass; break;
    case DefineFunctionOperator::ToCapture: this->mCurrentMode = Mode::ToCapture; break;
    case DefineFunctionOperator::WithContents: this->mCurrentMode = Mode::WithContents; break;
    default:
        AWESOME_THROW(SyntaxException) << "found unknown operator...";
        break;
    }
    env.pushScope(std::make_shared<DefineFunctionScope>(env.currentScope(), op));
    auto startNextMode = line.skipSpace(endKeyward);
    auto nextModeLine = Line(line.get(startNextMode), 0, line.length()-startNextMode);
    return this->parse(env, nextModeLine);
}

IParseMode::Result DefineFunctionParseMode::parseByPassMode(Enviroment& env, Line& line)
{
    assert(IScope::Type::DefineFunction == env.currentScope().type());
    auto& defineFunctionScope = dynamic_cast<DefineFunctionScope&>(env.currentScope());
    auto pCurrentScope = env.currentScopePointer();
    foreachArrayElement(line, 0, [&](auto line) {
        while (pCurrentScope != env.currentScopePointer()) {
            env.closeTopScope();
        }

        auto nameEnd = line.incrementPos(0, [](auto line, auto p) {
            return isNameChar(line.get(p));
        });
        auto nameView = line.substr(0, nameEnd);

        Argument arg;
        arg.name = nameView.to_string();

        auto pos = line.skipSpace(nameEnd);
        while (!line.isEndLine(pos)) {
            auto start = line.skipSpace(pos);
            auto end = line.incrementPos(start, [](auto line, auto p) {
                return !isSpace(line.get(p));
            });
            auto op = toArgumentOperator(line.substr(start, end-start));
            switch (op) {
            case ArgumentOperator::Is:
            {
                auto [s, e] = line.getRangeSeparatedBySpace(end);
                auto valueType = Value::toType(line.substr(s, e-s));
                if (valueType == Value::Type::None) {
                    AWESOME_THROW(SyntaxException) << "function argument is unknown type...";
                }
                arg.type = valueType;
                pos = e;
                break;
            }
            case ArgumentOperator::ByDefault:
            {
                auto valueType = (arg.type == Value::Type::None) ? Value::Type::String : arg.type;
                env.pushScope(std::make_shared<NormalScope>(std::list<std::string>{""}, Value().init(valueType)));
                end = line.skipSpace(end);
                if (!line.isEndLine(end)) {
                    auto valueLine = Line(line.get(end), 0, line.length() - end);
                    if (Value::Type::Array == valueType) {
                        parseArrayElement(env, valueLine, 0);
                    } else {
                        parseValue(env, valueLine);
                    }
                }
                env.pushMode(std::make_shared<NormalParseMode>());
                pos = line.length();
                break;
            }
            default:
                AWESOME_THROW(SyntaxException) << "unknown operator of function argument...";
            }
        }
        defineFunctionScope.addElememnt(arg);
        return true;
    });
    return Result::Continue;
}

IParseMode::Result DefineFunctionParseMode::parseByCaptureMode(Enviroment& env, Line& line)
{
    assert(IScope::Type::DefineFunction == env.currentScope().type());
    auto& defineFunctionScope = dynamic_cast<DefineFunctionScope&>(env.currentScope());
    foreachArrayElement(line, 0, [&](auto elementLine) {
        auto [headWordStart, headWordEnd] = elementLine.getRangeSeparatedBySpace(0);
        auto headWord = elementLine.substr(headWordStart, headWordEnd - headWordStart);
        bool isRef = false;
        size_t nameStart = 0;
        if (headWord == "ref") {
            isRef = true;
            nameStart = headWordEnd;
        }
        auto [nestNameView, nameEndPos] = parseName(elementLine, nameStart);
        auto nestName = toStringList(nestNameView);
        if (isRef) {
            Capture capture;
            capture.name = nestName.back();
            capture.value = Reference(&env, nestName);
            defineFunctionScope.addElememnt(std::move(capture));
        } else {
            Capture capture;
            capture.name = nestName.back();
            capture.value = *(const_cast<Enviroment const&>(env).searchValue(nestName, false));
            defineFunctionScope.addElememnt(std::move(capture));
        }
        return true;
    });
    return Result::Continue;
}

IParseMode::Result DefineFunctionParseMode::parseByContentsMode(Enviroment& env, Line& line)
{
    assert(IScope::Type::DefineFunction == env.currentScope().type());
    auto& defineFunctionScope = dynamic_cast<DefineFunctionScope&>(env.currentScope());
    defineFunctionScope.appendContentsLine(env, line.string_view().to_string());

    return Result::Continue;
}

void DefineFunctionParseMode::resetMode()
{
    this->mCurrentMode = Mode::Default;
}

}