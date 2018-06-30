#include "normal.h"

#include <iostream>

#include <boost/optional.hpp>

#include "../parserUtility.h"
#include "../line.h"
#include "../enviroment.h"

#include "multiLineComment.h"

using namespace std;

namespace parser
{

static ErrorHandle evalIndent(Enviroment& env, Line& line, int& outLevel);
static CommentType evalComment(Enviroment& env, Line& line);
static boost::optional<boost::string_view> pickupName(Line const& line, size_t start);
static std::list<boost::string_view> parseName(size_t& tailPos, Line const& line, size_t start);
static OperatorType parseOperator(size_t& outTailPos, Line const& line, size_t start);
static Value::Type parseValueType(Line const& line, size_t start, OperatorType opType);
static ErrorHandle closeTopScope(Enviroment& env);

void NormalParseMode::parse(Enviroment& env, Line& line)
{
    auto commentType = evalComment(env, line);
    if (CommentType::MultiLine == commentType) {
        return;
    }

    int level;
    if (auto error = evalIndent(env, line, level)) {
        cerr << error.message() << endl;
        return;
    }

    if (line.length() <= 0) {
        return;
    }

    int resultCompared = env.compareIndentLevel(level);
    if ( 0 < resultCompared) {
        cerr << env.source.row() << ": syntax error!! An indent above current scope depth is described.\n"
            << line.string_view() << endl;
        return;
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
            return;
        }

        auto opType = parseOperator(p, line, p);
        if (OperatorType::Unknown == opType) {
            cerr << env.source.row() << ": syntax error!! found unknown operater.\n"
                << line.string_view() << endl;
            return;
        }

        Value::Type valueType = parseValueType(line, p, opType);
        env.pushScope(Scope(nestNames, Value().init(valueType)));
        if (OperatorType::Are == opType) {
            env.pushScope(Scope(std::list<std::string>{ "" }, Value().init(Value::Type::Array)));
        }
        p = line.skipSpace(p);

        // parse value
        auto valueLine = Line(line.get(p), 0, line.length()-p);
        if ('[' == *valueLine.get(0)) {
            // decide the value to be Object.
            auto p = valueLine.incrementPos(1, [](auto line, auto p) { return ']' != *line.get(p); });
            if (valueLine.length() <= p) {
                cerr << env.source.row() << ": syntax error!! The object name is not encloded in square brackets([...]).\n"
                    << line.string_view() << endl;

                env.popScope();
                return;
            }
            auto rawObjectName = valueLine.substr(1, p);
            //auto nestNames = parseName(p, valueLine, 1);

            if (Value::Type::Array == env.currentScope().valueType()) {
                env.currentScope().value.pushValue(Value().init(Value::Type::Object));
            } else {
                env.currentScope().value.init(Value::Type::Object);
            }
        } else if ('\\' == *valueLine.get(0)) {
            env.currentScope().value.data = valueLine.substr(1, valueLine.length()-1).to_string();

        } else {
            auto str = valueLine.string_view().to_string();
            size_t pos;
            try {
                Value::number num = std::stod(str, &pos);
                if (str.size() == pos) {
                    env.currentScope().value = Value().init(Value::Type::Number);
                    env.currentScope().value.data = num;
                } else {
                    env.currentScope().value = Value().init(Value::Type::String);
                    env.currentScope().value.data = str;
                }
            } catch (std::invalid_argument& ) {
                env.currentScope().value = Value().init(Value::Type::String);
                env.currentScope().value.data = str;
            }
        }

        cout << env.source.row() << "," << env.indent.currentLevel() << "," << env.scopeStack.size() << ":"
            << " name=";
        for (auto&& n : nestNames) {
            cout << n << ".";
        }
        cout << " op=" << toString(opType) << ": scopeLevel=" << env.scopeStack.size() << endl;

    } else if (Value::Type::String == env.currentScope().valueType()) {
        env.currentScope().value.appendStr(line.string_view());

    } else if (Value::Type::Number == env.currentScope().valueType()) {
        // Convert Number to String when scope is multiple line.
        auto str = Value().init(Value::Type::String);
        str.data = std::to_string(env.currentScope().value.get<Value::number>());
        env.currentScope().value = str;

    } else {
        cout << env.source.row() << "," << env.indent.currentLevel() << "," << env.scopeStack.size() << ":"
            << Value::toString(env.currentScope().valueType()) << endl;
    }
}

ErrorHandle evalIndent(Enviroment& env, Line& line, int& outLevel)
{
    auto indent = line.getIndent();
    auto level = env.indent.calLevel(indent); 

    if (-1 == level) {
        if (!line.find(0, [](auto line, auto p) {  return !isSpace(line.get(p)); })) {
            // skip if blank line
            return {};
        }

        if (0 == env.indent.currentLevel()) {
            env.indent.setUnit(indent);
        } else {
            return MakeErrorHandle(env.source.row()) << "invalid indent";
        }
    } else {
        if (1 == level) {
            env.indent.setUnit(indent);
        }
        env.indent.setLevel(level);
    }

    line.resize(indent.size(), 0);
    outLevel = env.indent.currentLevel();
    return {};
}

CommentType evalComment(Enviroment& env, Line& line)
{
    if (isCommentChar(line.get(0))) {
        if (2 <= line.length() && isCommentChar(line.get(1))) {
            //if multiple line comment
            auto p = line.incrementPos(static_cast<size_t>(2), [](auto line, auto p) { return isCommentChar(line.get(p)); });
            env.pushMode(std::make_shared<MultiLineCommentParseMode>(static_cast<int>(p)));
            return CommentType::MultiLine;
        } else {
            //if single line comment
            line.resize(0, line.length());
            return CommentType::SingleLine;
        }
    } else if (isCommentChar(line.rget(0))) {
        // check comment at the end of the line.
        // count '#' at the end of line.
        size_t p = line.incrementPos(1, [](auto line, auto p) {
            return isCommentChar(line.rget(p));
        });

        auto const KEYWARD_COUNT = p;
        // search pair '###...'
        size_t count = 0;
        for (count = 0; !line.isEndLine(p); ++p) {
            if (count == KEYWARD_COUNT
                && false == isCommentChar(line.rget(p))) {
                break;
            }

            count = isCommentChar(line.rget(p)) ? count + 1 : 0;
        }
        if (count == KEYWARD_COUNT) {
            // remove space characters before comment.
            p = line.incrementPos(p, [](auto line, auto p) {
                return isSpace(line.rget(p));
            });
            line.resize(0, p);
        }
        return CommentType::EndOfLine;
    }
    return CommentType::None;
}

boost::optional<boost::string_view> pickupName(Line const& line, size_t start)
{
    // search including illegal characters
    auto p = line.incrementPos(start, [](auto line, auto p) {
        auto c = line.get(p);
        return !(isSpace(c) || isChildOrderAccessorString(c) || isParentOrderAccessorChar(c));
    });
    auto nameStr = Line(line.get(start), 0, p - start);
    if ( nameStr.find(0, [](auto line, auto p) { return !isNameChar(line.get(p)); }) ) {
        return boost::none;
    }
    return nameStr.string_view();
}

struct INameAccessorParseTraits
{
    enum class Type {
        None,
        ParentOrder,
        ChildOrder,
    };

    virtual Type type()const = 0;
    virtual bool isAccessKeyward(Line const&, size_t) const= 0;
    virtual void push(std::list<boost::string_view>&, boost::string_view const&) const = 0;
    virtual size_t skipAccessChars(Line const&, size_t) const = 0;
};

struct ParentOrderAccessorParseTraits final : public INameAccessorParseTraits
{
    Type type()const override { return Type::ParentOrder; }
    bool isAccessKeyward(Line const& line, size_t p) const override{
        return isParentOrderAccessorChar(line.get(p));
    }
    void push(std::list<boost::string_view>& out, boost::string_view const& name) const override {
        out.push_back(name);
    }
    size_t skipAccessChars(Line const& line, size_t p) const override {
        return line.incrementPos(p, [](auto line, auto p) { return !isNameChar(line.get(p)); });
    }

    static ParentOrderAccessorParseTraits const& instance() {
        static ParentOrderAccessorParseTraits const inst;
        return inst;
    }
};

struct ChildOrderAccessorParseTraits final : public INameAccessorParseTraits
{
    Type type()const override { return Type::ChildOrder; }
    bool isAccessKeyward(Line const& line, size_t p) const override {
        return isChildOrderAccessorString(boost::string_view(line.get(p), line.length() - p));
    }
    void push(std::list<boost::string_view>& out, boost::string_view const& name) const override {
        out.push_front(name);
    }
    size_t skipAccessChars(Line const& line, size_t p) const override {
        return line.incrementPos(p, [](auto line, auto p) { return !isSpace(line.get(p)); });
    }

    static ChildOrderAccessorParseTraits const& instance() {
        static ChildOrderAccessorParseTraits const inst;
        return inst;
    }
};

std::list<boost::string_view> parseName(size_t& tailPos, Line const& line, size_t start)
{
    size_t p = start;
    auto firstNameView = pickupName(line, p);
    if (!firstNameView) {
        return {};
    }
    p += firstNameView->length();
    tailPos = p;

    INameAccessorParseTraits const* pAccessorParser = nullptr;
    p = line.skipSpace(p);
    if (isParentOrderAccessorChar(line.get(p))) {
        // the name of the parent order
        pAccessorParser = &ParentOrderAccessorParseTraits::instance();
    } else if (isChildOrderAccessorString(line.substr(p, 2))) {
        // the name of the children order
        pAccessorParser = &ChildOrderAccessorParseTraits::instance();
    } else {
        return { *firstNameView };
    }
    p = pAccessorParser->skipAccessChars(line, p);

    std::list<boost::string_view> result;
    result.push_back(*firstNameView);
    while (!line.isEndLine(p)) {
        p = line.skipSpace(p);
        auto debugLine = Line(line.get(p), 0, line.length() - p);
        auto nameView = pickupName(line, p);
        if (!nameView) {
            return {};
        }
        pAccessorParser->push(result, *nameView);
        p += nameView->length();
        p = line.skipSpace(p);

        if (!pAccessorParser->isAccessKeyward(line, p)) {
            break;
        }
        p = pAccessorParser->skipAccessChars(line, p);
    }

    tailPos = p;
    return result;
}

OperatorType parseOperator(size_t& outTailPos, Line const& line, size_t start)
{
    start = line.skipSpace(start);
    auto p = line.incrementPos(start, [](auto line, auto p) { return !isSpace(line.get(p)); });
    auto opType = toOperatorType(line.substr(start, p - start));
    outTailPos = p;
    return opType;
}

Value::Type parseValueType(Line const& line, size_t start, OperatorType opType)
{
    if (opType == OperatorType::Are) {
        return Value::Type::Array;
    }

    return Value::Type::String;
}

ErrorHandle closeTopScope(Enviroment& env)
{
    Scope currentScope = env.currentScope();
    env.popScope();

    Value* pParentValue = nullptr;
    if (2 <= currentScope.nestName.size()) {
        // search parent value
        auto rootName = currentScope.nestName.front();
        for (auto scopeIt = env.scopeStack.rbegin(); env.scopeStack.rend() != scopeIt; ++scopeIt) {
            if (scopeIt->nestName.back() == rootName) {
                pParentValue = &scopeIt->value;
                break;
            }
            if (scopeIt->value.isExsitChild(rootName)) {
                ErrorHandle error;
                auto& childValue = scopeIt->value.getChild(rootName, error);
                if (error) {
                    return ErrorHandle(env.source.row(), std::move(error));
                }
                pParentValue = &childValue;
                break;
            }
        }
        if (nullptr == pParentValue) {
            return MakeErrorHandle(env.source.row())
                << "scope searching error!! Don't found '"<< rootName << "' in scope stack.";
        }

        //
        auto nestNameIt = ++currentScope.nestName.begin();
        auto endIt = --currentScope.nestName.end();
        for (; endIt != nestNameIt; ++nestNameIt) {
            ErrorHandle error;
            auto& childValue = pParentValue->getChild(*nestNameIt, error);
            if (error) {
                if (Value::Type::Object == pParentValue->type) {
                    auto it = currentScope.nestName.begin();
                    auto name = *it;
                    for (++it; nestNameIt != it; ++it) {
                        name = "." + (*it);
                    }
                    return MakeErrorHandle(env.source.row())
                        << error.message() << "n"
                        << "scope searching error!! Don't found '" << *nestNameIt << "' in '" << name << "'.";
                } else {
                    return ErrorHandle(env.source.row(), std::move(error));
                }
            }
            pParentValue = &childValue;
        }
    } else {
        pParentValue = &env.currentScope().value;
    }

    switch (pParentValue->type) {
    case Value::Type::Object:
        pParentValue->addMember(currentScope);
        break;
    case Value::Type::Array:
        pParentValue->pushValue(currentScope.value);
        break;
    default:
        return MakeErrorHandle(env.source.row()) << "syntax error!! The current value can not have children.";
    }

    return {};
}

}