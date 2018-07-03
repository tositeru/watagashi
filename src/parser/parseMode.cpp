#include "parseMode.h"

#include <iostream>

#include "enviroment.h"
#include "line.h"
#include "parserUtility.h"
#include "value.h"
#include "mode/multiLineComment.h"

using namespace std;

namespace parser
{

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

ErrorHandle closeTopScope(Enviroment& env)
{
    // note: At the timing of closing the scope,
    //  we assign values to appropriate places.
    // etc) object member, array element or other places.

    auto pCurrentScope = env.currentScopePointer();
    env.popScope();
    if (IScope::Type::Reference == pCurrentScope->type()) {
        return {};
    }
    if (Value::Type::ObjectDefined == pCurrentScope->valueType()) {
        env.popMode();
    }

    Value* pParentValue = nullptr;
    if (2 <= pCurrentScope->nestName().size()) {
        if (auto error = searchValue(&pParentValue, pCurrentScope->nestName(), env, true)) {
            return error;
        }
    } else {
        pParentValue = &env.currentScope().value();
    }

    switch (pParentValue->type) {
    case Value::Type::Object:
        if (!pParentValue->addMember(*pCurrentScope)) {
            return MakeErrorHandle(env.source.row()) << "syntax error!! Failed to add an element to the current scope object.";
        }
        break;
    case Value::Type::Array:
        if ("" == pCurrentScope->nestName().back()) {
            pParentValue->pushValue(pCurrentScope->value());
        } else {
            if (!pParentValue->addMember(*pCurrentScope)) {
                return MakeErrorHandle(env.source.row()) << "syntax error!! Failed to add an element to the current scope array because it was a index out of range.";
            }
        }
        break;
    case Value::Type::ObjectDefined:
        if (!pParentValue->addMember(*pCurrentScope)) {
            return MakeErrorHandle(env.source.row()) << "syntax error!! Failed to add an element to the current scope object.";
        }
        break;
    case Value::Type::MemberDefined:
    {
        auto& memberDefined = pParentValue->get<MemberDefined>();
        memberDefined.defaultValue = pCurrentScope->value();
        break;
    }
    default:
        return MakeErrorHandle(env.source.row()) << "syntax error!! The current value can not have children.";
    }

    return {};
}

size_t parseArrayElement(Enviroment& env, Line& line, size_t start)
{
    auto valuePos = line.skipSpace(start);
    if (line.isEndLine(valuePos)) {
        return valuePos;
    }

    auto getTail = [](Line& line, size_t start) -> size_t {
        // search explicit separator of string array element 
        for (auto p = start; !line.isEndLine(p + 1); ++p) {
            auto strView = boost::string_view(line.get(p), 2);
            if (isExplicitStringArrayElementSeparater(strView)) {
                return p;
            }
        }

        return  line.incrementPos(start, [](auto line, auto p) {
            return !isArrayElementSeparater(line.get(p)); });
    };
    auto tail = getTail(line, valuePos);

    do {
        auto valueLine = Line(line.get(valuePos), 0, tail - valuePos);
        env.pushScope(std::make_shared<NormalScope>(std::list<std::string>{""}, Value()));
        if (auto error = parseValue(env, valueLine)) {
            cerr << error.message()
                << line.string_view() << endl;
            env.popScope();
            return valuePos;
        }

        tail += ('\\' == *line.get(tail)) ? 2 : 1;
        valuePos = line.skipSpace(tail);

        if (line.isEndLine(valuePos)) {
            break;
        }
        tail = getTail(line, valuePos);

        if (auto error = closeTopScope(env)) {
            cerr << error.message() << "\n"
                << line.string_view() << endl;
        }
    } while (!line.isEndLine(valuePos));
    return valuePos;
}

ErrorHandle parseObjectName(boost::string_view& out, size_t& outTail, Enviroment& env, Line& line, size_t start)
{
    auto nameLine = Line(line.get(start), 0, line.length() - start);
    if ('[' != *nameLine.get(0)) {
        return MakeErrorHandle(env.source.row())
            << env.source.row() << ": syntax error!! The object name is not encloded in square brackets([...]).\n";
    }

    // decide the value to be Object.
    auto p = nameLine.incrementPos(1, [](auto line, auto p) { return ']' != *line.get(p); });
    if (nameLine.length() <= p) {
        return MakeErrorHandle(env.source.row())
            << env.source.row() << ": syntax error!! The object name is not encloded in square brackets([...]).\n";
    }

    out = nameLine.substr(1, p - 1);
    outTail = p;
    return {};
}

ErrorHandle parseValue(Enviroment& env, Line& valueLine)
{
    auto start = valueLine.skipSpace(0);
    valueLine.resize(start, 0);
    if ('[' == *valueLine.get(0)) {
        boost::string_view rawObjectName;
        size_t p=0;
        if (auto error = parseObjectName(rawObjectName, p, env, valueLine, 0)) {
            return error;
        }

        if ("Array" == rawObjectName) {
            env.currentScope().value().init(Value::Type::Array);
            auto startArrayElement = valueLine.skipSpace(p + 1);
            parseArrayElement(env, valueLine, startArrayElement);
        } else {
            env.currentScope().value().init(Value::Type::Object);
        }

    } else if ('\\' == *valueLine.get(0)) {
        env.currentScope().value() = valueLine.substr(1, valueLine.length() - 1).to_string();

    } else {
        auto str = valueLine.string_view().to_string();
        bool isNumber = false;
        auto num = toDouble(str, isNumber);
        if (isNumber) {
            env.currentScope().value() = num;
        } else {
            env.currentScope().value() = str;
        }
    }

    return {};
}

boost::optional<boost::string_view> pickupName(Line const& line, size_t start)
{
    // search including illegal characters
    auto p = line.incrementPos(start, [](auto line, auto p) {
        auto c = line.get(p);
        return !(isSpace(c) || isParentOrderAccessorChar(c));
    });
    auto nameStr = Line(line.get(start), 0, p - start);
    if (nameStr.find(0, [](auto line, auto p) { return !isNameChar(line.get(p)); })) {
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
    virtual bool isAccessKeyward(Line const&, size_t) const = 0;
    virtual void push(std::list<boost::string_view>&, boost::string_view const&) const = 0;
    virtual size_t skipAccessChars(Line const&, size_t) const = 0;
};

struct ParentOrderAccessorParseTraits final : public INameAccessorParseTraits
{
    Type type()const override { return Type::ParentOrder; }
    bool isAccessKeyward(Line const& line, size_t p) const override {
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
        // the name of the child order
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

ErrorHandle searchValue(Value** ppOut, std::list<std::string> const& nestName, Enviroment& env, bool doGetParent)
{
    assert(ppOut != nullptr);
    assert(!nestName.empty());

    Value* pResult = nullptr;

    // Find the starting point of the appropriate place.
    auto rootName = nestName.front();
    for (auto pScopeIt = env.scopeStack.rbegin(); env.scopeStack.rend() != pScopeIt; ++pScopeIt) {
        if ((*pScopeIt)->nestName().back() == rootName) {
            pResult = &(*pScopeIt)->value();
            break;
        }
        if ((*pScopeIt)->value().isExsitChild(rootName)) {
            ErrorHandle error;
            auto& childValue = (*pScopeIt)->value().getChild(rootName, error);
            if (error) {
                return ErrorHandle(env.source.row(), std::move(error));
            }
            pResult = &childValue;
            break;
        }
    }
    if (nullptr == pResult) {
        return MakeErrorHandle(env.source.row())
            << "scope searching error!! Don't found '" << rootName << "' in scope stack.";
    }

    // Find a assignment destination at starting point.
    if (2 <= nestName.size()) {
        auto nestNameIt = ++nestName.begin();
        auto endIt = nestName.end();
        if (doGetParent) {
            --endIt;
        }

        for (; endIt != nestNameIt; ++nestNameIt) {
            ErrorHandle error;
            auto& childValue = pResult->getChild(*nestNameIt, error);
            if (error) {
                if (Value::Type::Object == pResult->type) {
                    auto it = nestName.begin();
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
            pResult = &childValue;
        }
    }

    *ppOut = pResult;
    return {};
}

Value::Type parseValueType(Enviroment& env, Line& line, size_t& inOutPos)
{
    auto p = line.skipSpace(inOutPos);
    auto end = line.incrementPos(p, [](auto line, auto p) {
        return isNameChar(line.get(p));
    });
    auto typeStrView = line.substr(p, end - p);
    auto type = Value::toType(typeStrView);
    inOutPos = end;
    return type;
}

MemberDefinedOperatorType parseMemberDefinedOperator(size_t& outTailPos, Line const& line, size_t start)
{
    start = line.skipSpace(start);
    auto p = line.incrementPos(start, [](auto line, auto p) { return !isSpace(line.get(p)); });
    auto opType = toMemberDefinedOperatorType(line.substr(start, p - start));
    outTailPos = p;
    return opType;
}

}