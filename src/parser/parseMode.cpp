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
    } else if (Value::Type::ObjectDefined == pCurrentScope->valueType()) {
        env.popMode();
    } else if (Value::Type::Object == pCurrentScope->valueType()) {
        auto& obj = pCurrentScope->value().get<Value::object>();
        if (auto error = obj.applyObjectDefined()) {
            std::string fullname = "";
            std::string accesser = "";
            for (auto&& name : pCurrentScope->nestName()) {
                fullname += accesser + name;
                accesser = ".";
            }
            return MakeErrorHandle(env.source.row())
                << "defined object error!! : An undefined member exists in '" << fullname << "'.\n"
                << error.message();
        }
    } else if (Value::Type::String == pCurrentScope->valueType()) {
        auto& str = pCurrentScope->value().get<Value::string>();
        if (auto error = expandVariable(str, env)) {
            return error;
        }
    } else if (Value::Type::Array == pCurrentScope->valueType()) {
        // TODO check reference value
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

ErrorHandle parseObjectName(std::list<boost::string_view>& out, size_t& outEnd, Enviroment& env, Line& line, size_t start)
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

    nameLine.resize(1, nameLine.length()-p);
    size_t endPos = 0;
    auto nestName = parseName(endPos, nameLine, 0);
    out = std::move(nestName);
    outEnd = endPos+1 + start;
    return {};
}

ErrorHandle searchObjdectDefined(ObjectDefined const** ppOut, std::list<boost::string_view> const& nestName, Enviroment const& env)
{
    if (1 == nestName.size()) {
        if ("Array" == *nestName.begin()) {
            *ppOut = &Value::arrayDefined;
            return {};
        } else if ("Object" == *nestName.begin()) {
            *ppOut = &Value::objectDefined;
            return {};
        }
    }
    auto nestNameList = toStringList(nestName);
    Value const* pValue=nullptr;
    if (auto error = searchValue(&pValue, nestNameList, env)) {
        return MakeErrorHandle(env.source.row())
            << "scope searching error: Don't found ObjectDefined.\n";
    }

    if (Value::Type::ObjectDefined != pValue->type) {
        return MakeErrorHandle(env.source.row())
            << "scope searching error: Value is not ObjectDefined.\n";
    }
    *ppOut = &pValue->get<ObjectDefined>();
    return {};
}

ErrorHandle parseValue(Enviroment& env, Line& valueLine)
{
    auto start = valueLine.skipSpace(0);
    valueLine.resize(start, 0);
    if ('[' == *valueLine.get(0)) {
        size_t p = 0;
        std::list<boost::string_view> objectNestName;
        if (auto error = parseObjectName(objectNestName, p, env, valueLine, 0)) {
            return error;
        }
        ObjectDefined const* pObjectDefined = nullptr;
        if (auto error = searchObjdectDefined(&pObjectDefined, objectNestName, env)) {
            return error;
        }
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
        env.currentScope().value() = valueLine.substr(1, valueLine.length() - 1).to_string();

    } else {
        auto str = valueLine.string_view().to_string();
        bool isNumber = false;
        auto num = toDouble(str, isNumber);
        if (isNumber) {
            env.currentScope().value() = num;
        } else {
            if (isReference(str)) {
                auto strLine = Line(&str[2], 0,str.size()-3);
                size_t endPos;
                auto nestName = toStringList(parseName(endPos, strLine, 0));
                env.currentScope().value() = Reference(&env, nestName);
            } else {
                env.currentScope().value() = str;
            }
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

ErrorHandle searchValue(Value** ppOut, std::list<std::string> const& nestName, Enviroment const& env, bool doGetParent)
{
    Value const*pValue;
    if (auto error = searchValue(&pValue, nestName, env, doGetParent)) {
        return error;
    }
    *ppOut = const_cast<Value*>(pValue);
    return {};
}

ErrorHandle searchValue(Value const** ppOut, std::list<std::string> const& nestName, Enviroment const& env, bool doGetParent)
{
    assert(ppOut != nullptr);
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
            ErrorHandle error;
            auto& childValue = pScope->value().getChild(rootName, error);
            if (error) {
                return ErrorHandle(env.source.row(), std::move(error));
            }
            pResult = &childValue;
            break;
        }
    }
    if (nullptr == pResult) {
        ErrorHandle error;
        pResult = &env.externObj.getChild(rootName, error);
        if (error) {
            return MakeErrorHandle(env.source.row())
                << "scope searching error!! Don't found '" << rootName << "' in enviroment.";
        }
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

ErrorHandle expandVariable(std::string & inOutStr, Enviroment const& env)
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
        size_t lineEnd;
        auto nestName = toStringList(parseName(lineEnd, strLine, 0));
        Value const* pValue = nullptr;
        if (auto error = searchValue(&pValue, nestName, env)) {
            return error;
        }
        switch (pValue->type) {
        case Value::Type::String:
            str.replace(start, end - start + 1, pValue->get<Value::string>());
            break;
        case Value::Type::Number:
            str.replace(start, end + 1, pValue->toString());
            break;
        default:
            return MakeErrorHandle(env.source.row())
                << "syntax error!! Don't expansion type '" << Value::toString(pValue->type) << "'.";
            break;
        }
    }

    inOutStr = str;
    return {};
}

}