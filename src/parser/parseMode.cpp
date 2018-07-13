#include "parseMode.h"

#include <iostream>

#include <boost/range/adaptor/reversed.hpp>

#include "enviroment.h"
#include "line.h"
#include "parserUtility.h"
#include "value.h"
#include "mode/multiLineComment.h"

using namespace std;

namespace parser
{

IParseMode::Result IParseMode::preprocess(Enviroment& env, Line& line)
{
    auto commentType = evalComment(env, line);
    if (CommentType::MultiLine == commentType) {
        return Result::NextLine;
    }

    int level = evalIndent(env, line);
    if (line.length() <= 0) {
        return Result::NextLine;
    }

    int resultCompared = env.compareIndentLevel(level);
    if (0 < resultCompared) {
        throw MakeException<SyntaxException>()
            << "An indent above current scope depth is described." << MAKE_EXCEPTION;
    }

    if (resultCompared < 0) {
        while (0 != env.compareIndentLevel(level)) {
            // close top scope.
            closeTopScope(env);
        }
        if (env.currentMode().get() != this) {
            return Result::Redo;
        }
    }
    return Result::Continue;
}

int evalIndent(Enviroment& env, Line& line)
{
    auto indent = line.getIndent();
    auto level = env.indent.calLevel(indent);

    if (-1 == level) {
        if (!line.find(0, [](auto line, auto p) {  return !isSpace(line.get(p)); })) {
            // skip if blank line
            return env.indent.currentLevel();
        }

        if (0 == env.indent.currentLevel()) {
            env.indent.setUnit(indent);
        } else {
            throw MakeException<SyntaxException>()
                << "invalid indent" << MAKE_EXCEPTION;
        }
    } else {
        if (1 == level) {
            env.indent.setUnit(indent);
        }
        env.indent.setLevel(level);
    }

    line.resize(indent.size(), 0);
    return env.indent.currentLevel();
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

void closeTopScope(Enviroment& env)
{
    // note: At the timing of closing the scope,
    //  we assign values to appropriate places.
    // etc) object member, array element or other places.

    auto pCurrentScope = env.currentScopePointer();
    env.popScope();
    if (IScope::Type::Reference == pCurrentScope->type()) {
        return;
    } else if(IScope::Type::Boolean == pCurrentScope->type()) {
        auto pBooleanScope = dynamic_cast<BooleanScope*>(pCurrentScope.get());
        pBooleanScope->value() = pBooleanScope->result();
        env.popMode();

    } else if (Value::Type::ObjectDefined == pCurrentScope->valueType()) {
        auto& objectDefined = pCurrentScope->value().get<ObjectDefined>();
        objectDefined.name = pCurrentScope->nestName().back();
        env.popMode();
    } else if (Value::Type::Object == pCurrentScope->valueType()) {
        auto& obj = pCurrentScope->value().get<Value::object>();
        if (!obj.applyObjectDefined()) {
            std::string fullname = toNameString(pCurrentScope->nestName());
            throw MakeException<DefinedObjectException>()
                << "An undefined member exists in '" << fullname << "." << MAKE_EXCEPTION;
        }
    } else if (Value::Type::String == pCurrentScope->valueType()) {
        auto& str = pCurrentScope->value().get<Value::string>();
        str = expandVariable(str, env);
    }

    Value* pParentValue = nullptr;
    if (2 <= pCurrentScope->nestName().size()) {
        pParentValue = searchValue(pCurrentScope->nestName(), env, true);
    } else {
        pParentValue = &env.currentScope().value();
    }

    switch (pParentValue->type) {
    case Value::Type::Object:
        if (!pParentValue->addMember(*pCurrentScope)) {
            throw MakeException<SyntaxException>()
                << "Failed to add an element to the current scope object." << MAKE_EXCEPTION;
        }

        break;
    case Value::Type::Array:
        if ("" == pCurrentScope->nestName().back()) {
            pParentValue->pushValue(pCurrentScope->value());
        } else {
            if (!pParentValue->addMember(*pCurrentScope)) {
                throw MakeException<SyntaxException>()
                    << "Failed to add an element to the current scope array because it was a index out of range."
                    << MAKE_EXCEPTION;
            }
        }
        break;
    case Value::Type::ObjectDefined:
        if (!pParentValue->addMember(*pCurrentScope)) {
            throw MakeException<SyntaxException>()
                << "Failed to add an element to the current scope object."
                << MAKE_EXCEPTION;
        }
        break;
    case Value::Type::MemberDefined:
    {
        auto& memberDefined = pParentValue->get<MemberDefined>();
        memberDefined.defaultValue = pCurrentScope->value();
        break;
    }
    default:
        throw MakeException<SyntaxException>()
            << "The current value can not have children."
            << MAKE_EXCEPTION;
    }
}

size_t parseArrayElement(Enviroment& env, Line& line, size_t start)
{
    auto valuePos = line.skipSpace(start);
    if (line.isEndLine(valuePos)) {
        return valuePos;
    }

    auto getEndPos = [](Line& line, size_t start) -> size_t {
        // search explicit separator of string array element 
        for (auto p = start; !line.isEndLine(p + 1); ++p) {
            auto strView = boost::string_view(line.get(p), 2);
            if (isExplicitStringArrayElementSeparater(strView)) {
                return p;
            }
        }

        return line.incrementPos(start, [](auto line, auto p) {
                return !isArrayElementSeparater(line.get(p));
            });
    };
    auto endPos = getEndPos(line, valuePos);

    do {
        auto valueLine = Line(line.get(valuePos), 0, endPos - valuePos);
        env.pushScope(std::make_shared<NormalScope>(std::list<std::string>{""}, Value()));
        parseValue(env, valueLine);

        endPos += ('\\' == *line.get(endPos)) ? 2 : 1;
        valuePos = line.skipSpace(endPos);

        if (line.isEndLine(valuePos)) {
            break;
        }
        endPos = getEndPos(line, valuePos);

        closeTopScope(env);
    } while (!line.isEndLine(valuePos));
    return valuePos;
}

std::tuple<std::list<boost::string_view>, size_t> parseObjectName(Enviroment const& env, Line& line, size_t start)
{
    auto nameLine = Line(line.get(start), 0, line.length() - start);
    if ('[' != *nameLine.get(0)) {
        throw MakeException<SyntaxException>()
            << "The object name is not encloded in square brackets([...])."
            << MAKE_EXCEPTION;
    }

    // decide the value to be Object.
    auto p = nameLine.incrementPos(1, [](auto line, auto p) { return ']' != *line.get(p); });
    if (nameLine.length() <= p) {
        throw MakeException<SyntaxException>()
            << "The object name is not encloded in square brackets([...])."
            << MAKE_EXCEPTION;
    }
    nameLine.resize(1, nameLine.length()-p);
    auto [nestName, endPos] = parseName(nameLine, 0);
    endPos = endPos + 1 + start;
    return { std::move(nestName), endPos };
}

ObjectDefined const* searchObjdectDefined(std::list<boost::string_view> const& nestName, Enviroment const& env)
{
    if (1 == nestName.size()) {
        if ("Array" == *nestName.begin()) {
            return &Value::arrayDefined;
        } else if ("Object" == *nestName.begin()) {
            return &Value::objectDefined;
        }
    }
    auto nestNameList = toStringList(nestName);
    Value const* pValue= searchValue(nestNameList, env);
    if (Value::Type::ObjectDefined != pValue->type) {
        throw MakeException<ScopeSearchingException>()
            << "Value is not ObjectDefined." << MAKE_EXCEPTION;
    }
    return &pValue->get<ObjectDefined>();
}

void parseValue(Enviroment& env, Line& valueLine)
{
    auto start = valueLine.skipSpace(0);
    valueLine.resize(start, 0);
    if ('[' == *valueLine.get(0)) {
        // parse Object
        auto [objectNestName, p] = parseObjectName(env, valueLine, 0);

        ObjectDefined const* pObjectDefined = searchObjdectDefined(objectNestName, env);
        if (&Value::arrayDefined == pObjectDefined) {
            env.currentScope().value().init(Value::Type::Array);
            auto startArrayElement = valueLine.skipSpace(p + 1);
            parseArrayElement(env, valueLine, startArrayElement);
        } else if(&Value::objectDefined == pObjectDefined) {
            env.currentScope().value().init(Value::Type::Object);
        } else {
            env.currentScope().value() = Object(pObjectDefined);
        }

    } else if ('\\' == *valueLine.get(0)) {
        // explicitly parse string
        env.currentScope().value() = valueLine.substr(1, valueLine.length() - 1).to_string();

    } else {
        // parse string, number or reference
        auto str = valueLine.string_view().to_string();
        bool isNumber = false;
        auto num = toDouble(str, isNumber);
        if (isNumber) {
            env.currentScope().value() = num;
        } else {
            if (isReference(str)) {
                auto strLine = Line(&str[2], 0,str.size()-3);
                auto [nestNameView, endPos] = parseName(strLine, 0);
                auto nestName = toStringList(nestNameView);
                env.currentScope().value() = Reference(&env, nestName);
            } else {
                env.currentScope().value() = str;
            }
        }
    }
}

Value parseValueInSingleLine(Enviroment const& env, Line& valueLine)
{
    auto start = valueLine.skipSpace(0);
    valueLine.resize(start, 0);
    if ('[' == *valueLine.get(0)) {
        // parse Object
        auto[objectNestName, p] = parseObjectName(env, valueLine, 0);

        ObjectDefined const* pObjectDefined = searchObjdectDefined(objectNestName, env);
        if (&Value::arrayDefined == pObjectDefined) {
            return Value().init(Value::Type::Array);
        } else if (&Value::objectDefined == pObjectDefined) {
            return Value().init(Value::Type::Object);
        } else {
            return Object(pObjectDefined);
        }

    } else if ('\\' == *valueLine.get(0)) {
        // explicitly parse string
        return valueLine.substr(1, valueLine.length() - 1).to_string();

    } else {
        // parse string, number or reference
        auto str = valueLine.string_view().to_string();
        bool isNumber = false;
        auto num = toDouble(str, isNumber);
        if (isNumber) {
            return num;
        } else {
            if (isReference(str)) {
                auto strLine = Line(&str[2], 0, str.size() - 3);
                auto[nestNameView, endPos] = parseName(strLine, 0);
                auto nestName = toStringList(nestNameView);
                return Reference(&env, nestName);
            } else {
                return str;
            }
        }
    }

    return Value::none;
}

std::tuple<boost::string_view, bool> pickupName(Line const& line, size_t start)
{
    start = line.skipSpace(start);
    // search including illegal characters
    auto p = line.incrementPos(start, [](auto line, auto p) {
        auto c = line.get(p);
        return !(isSpace(c) || isParentOrderAccessorChar(c) || isContinueLogicOperatorChar(c));
    });
    auto nameStr = Line(line.get(start), 0, p - start);
    if (nameStr.find(0, [](auto line, auto p) { return !isNameChar(line.get(p)); })) {
        return { boost::string_view{}, false };
    }
    return { nameStr.string_view(), true };
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

std::tuple<std::list<boost::string_view>, size_t> parseName(Line const& line, size_t start, bool &outIsSuccess)
{
    auto[firstNameView, isSuccess] = pickupName(line, start);
    if (!isSuccess) {
        outIsSuccess = false;
        return { {}, 0 };
    }

    auto p = start + firstNameView.length();
    INameAccessorParseTraits const* pAccessorParser = nullptr;
    p = line.skipSpace(p);
    if (isParentOrderAccessorChar(line.get(p))) {
        // the name of the parent order
        pAccessorParser = &ParentOrderAccessorParseTraits::instance();
    } else if (isChildOrderAccessorString(line.substr(p, 2))) {
        // the name of the child order
        pAccessorParser = &ChildOrderAccessorParseTraits::instance();
    } else {
        auto endPos = start + firstNameView.length();
        outIsSuccess = true;
        return { { firstNameView }, endPos };
    }
    p = pAccessorParser->skipAccessChars(line, p);

    std::list<boost::string_view> result;
    result.push_back(firstNameView);
    while (!line.isEndLine(p)) {
        p = line.skipSpace(p);
        auto debugLine = Line(line.get(p), 0, line.length() - p);
        auto[nameView, isSuccess] = pickupName(line, p);
        if (!isSuccess) {
            outIsSuccess = false;
            return { {}, 0 };
        }
        pAccessorParser->push(result, nameView);
        p += nameView.length();
        p = line.skipSpace(p);

        if (!pAccessorParser->isAccessKeyward(line, p)) {
            break;
        }
        p = pAccessorParser->skipAccessChars(line, p);
    }

    outIsSuccess = true;
    return { result, p };
}

std::tuple<std::list<boost::string_view>, size_t> parseName(Line const& line, size_t start)
{
    bool isSuccess;
    auto result = parseName(line, start, isSuccess);
    if (!isSuccess) {
        AWESOME_THROW(SyntaxException) << "Failed to parse name...";
    }
    return result;
}

OperatorType parseOperator(size_t& outEndPos, Line const& line, size_t start)
{
    start = line.skipSpace(start);
    auto p = line.incrementPos(start, [](auto line, auto p) { return !isSpace(line.get(p)); });
    auto opType = toOperatorType(line.substr(start, p - start));
    outEndPos = p;
    return opType;
}

Value* searchValue(std::list<std::string> const& nestName, Enviroment& env, bool doGetParent)
{
    return const_cast<Value*>(searchValue(nestName, const_cast<Enviroment const&>(env), doGetParent));
}

Value const* searchValue(std::list<std::string> const& nestName, Enviroment const& env, bool doGetParent)
{
    assert(!nestName.empty());

    Value const* pResult = nullptr;

    // Find the starting point of the appropriate place.
    auto rootName = nestName.front();
    for (auto&& pScope : boost::adaptors::reverse(env.scopeStack)) {
        if (pScope->nestName().back() == rootName) {
            pResult = &pScope->value();
            break;
        }
        if (pScope->value().isExsitChild(rootName)) {
            auto& childValue = pScope->value().getChild(rootName);
            pResult = &childValue;
            break;
        }
    }
    if (nullptr == pResult) {
        if (!env.externObj.isExsitChild(rootName)) {
            throw MakeException<ScopeSearchingException>()
                << "Don't found '" << rootName << "' in enviroment."
                << MAKE_EXCEPTION;
        }
        pResult = &env.externObj.getChild(rootName);
    }

    // Find a assignment destination at starting point.
    if (2 <= nestName.size()) {
        auto nestNameIt = ++nestName.begin();
        auto endIt = nestName.end();
        if (doGetParent) {
            --endIt;
        }
        for (; endIt != nestNameIt; ++nestNameIt) {
            if (!pResult->isExsitChild(*nestNameIt)) {
                // error code
                auto it = nestName.begin();
                auto name = *it;
                for (++it; nestNameIt != it; ++it) {
                    name = "." + (*it);
                }
                if (Value::Type::Object == pResult->type) {
                    throw MakeException<ScopeSearchingException>()
                        << "Don't found '" << *nestNameIt << "' in '" << name << "'."
                        << MAKE_EXCEPTION;
                } else {
                    throw MakeException<ScopeSearchingException>()
                        << *nestNameIt << "' in '" << name << "' not equal Object."
                        << MAKE_EXCEPTION;
                }
            }

            pResult = &pResult->getChild(*nestNameIt);
        }
    }

    return pResult;
}

Value* searchValue(bool& outIsSuccess, std::list<std::string> const& nestName, Enviroment & env, bool doGetParent)
{
    return const_cast<Value*>(searchValue(outIsSuccess, nestName, const_cast<Enviroment const&>(env), doGetParent));
}

Value const* searchValue(bool& outIsSuccess, std::list<std::string> const& nestName, Enviroment const& env, bool doGetParent)
{
    try {
        auto pValue = searchValue(nestName, env, doGetParent);
        outIsSuccess = true;
        return pValue;
    } catch (...) {
        outIsSuccess = false;
        return nullptr;
    }
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

MemberDefinedOperatorType parseMemberDefinedOperator(size_t& outEndPos, Line const& line, size_t start)
{
    start = line.skipSpace(start);
    auto p = line.incrementPos(start, [](auto line, auto p) { return !isSpace(line.get(p)); });
    auto opType = toMemberDefinedOperatorType(line.substr(start, p - start));
    outEndPos = p;
    return opType;
}

std::string expandVariable(std::string & inOutStr, Enviroment const& env)
{
    auto str = inOutStr;
    size_t start = 0;
    while (true) {
        start = str.find("${", start);
        if (std::string::npos == start) {
            break;
        }
        auto end = str.find('}', start);
        if (std::string::npos == end) {
            break;
        }
        auto strLine = Line(&str[start+2], 0, end - (start+2));
        auto [nestNameView, linEnd] = parseName(strLine, 0);
        auto nestName = toStringList(nestNameView);
        Value const* pValue = searchValue(nestName, env);
        switch (pValue->type) {
        case Value::Type::String:
            str.replace(start, end - start + 1, pValue->get<Value::string>());
            break;
        case Value::Type::Number:
            str.replace(start, end + 1, pValue->toString());
            break;
        default:
            throw MakeException<SyntaxException>()
                << "Don't expansion type '" << Value::toString(pValue->type) << "'."
                << MAKE_EXCEPTION;
            break;
        }
    }

    return str;
}

std::tuple<CompareOperator, size_t> parseCompareOperator(Line& line, size_t start)
{
    start = line.skipSpace(start);
    auto p = line.incrementPos(start, [](auto line, auto p) { return !isSpace(line.get(p)); });
    auto opType = toCompareOperatorType(line.substr(start, p - start));
    return { opType, p};
}

}