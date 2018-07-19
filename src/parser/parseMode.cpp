#include "parseMode.h"

#include <iostream>

#include <boost/range/adaptor/reversed.hpp>

#include "enviroment.h"
#include "line.h"
#include "parserUtility.h"
#include "value.h"
#include "mode/multiLineComment.h"
#include "mode/normal.h"
#include "mode/doNothing.h"

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
        switch (env.currentScope().type()) {
        case IScope::Type::Branch:
        {
            if (2 <= resultCompared) {
                throw MakeException<SyntaxException>()
                    << "An indent above current scope depth is described." << MAKE_EXCEPTION;
            }

            auto& branchScope = dynamic_cast<BranchScope&>(env.currentScope());
            if (branchScope.doCurrentStatements()) {
                env.pushScope(std::make_shared<ReferenceScope>(branchScope.nestName(), branchScope.value(), true));
                env.pushMode(std::make_shared<NormalParseMode>());
                branchScope.incrementRunningCount();

            } else {
                env.pushMode(std::make_shared<DoNothingParseMode>());
                env.pushScope(std::make_shared<DummyScope>());

            }
            branchScope.resetBranchState();
            return Result::Continue;
        }
        default:
            throw MakeException<SyntaxException>()
                << "An indent above current scope depth is described." << MAKE_EXCEPTION;
            break;
        }
    }

    if (resultCompared < 0) {
        while (0 != env.compareIndentLevel(level)) {
            // close top scope.
            env.closeTopScope();
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

std::tuple<StartPos, EndPos> searchArraySeparaterPos(Line const& line, size_t start)
{
    // search explicit separator of string array element 
    for (auto p = start; !line.isEndLine(p + 1); ++p) {
        auto strView = boost::string_view(line.get(p), 2);
        if (isExplicitStringArrayElementSeparater(strView)) {
            auto startPos = line.decrementPos(p, [](auto line, auto p) { return !isSpace(line.get(p)); });
            return { startPos, p + strView.length() };
        }
    }

    auto end = line.incrementPos(start, [](auto line, auto p) {
        return !isArrayElementSeparater(line.get(p));
    });
    auto startPos = line.decrementPos(end, [](auto line, auto p) { return !isSpace(line.get(p)); });
    return { startPos, end };
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

        env.closeTopScope();
    } while (!line.isEndLine(valuePos));
    return valuePos;
}

std::tuple<std::list<boost::string_view>, EndPos> parseObjectName(Enviroment const& env, Line& line, size_t start)
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
    if (nameStr.length() <= 0 || nameStr.find(0, [](auto line, auto p) { return !isNameChar(line.get(p)); })) {
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

std::tuple<std::list<boost::string_view>, EndPos> parseName(Line const& line, size_t start, bool &outIsSuccess)
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

std::tuple<std::list<boost::string_view>, EndPos> parseName(Line const& line, size_t start)
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

RefOrEntityValue getValue(Enviroment const& env, Line& valueLine)
{
    bool isSuccess = false;
    auto[nestNameView, leftValueNamePos] = parseName(valueLine, 0, isSuccess);
    Value const* pValue = nullptr;
    if (isSuccess) {
        pValue = searchValue(isSuccess, toStringList(nestNameView), env);
    }
    if (!pValue) {
        return parseValueInSingleLine(env, valueLine);
    }
    return RefOrEntityValue(pValue);
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

std::tuple<Value const*, bool> parseBool(Enviroment const& env, Line const& line)
{
    //check denial
    auto[isDenial, dinialEndPos] = doExistDenialKeyward(line);
    //
    auto[boolVarNestName, nameEndPos] = parseName(line, dinialEndPos);
    Value const* pValue = searchValue(toStringList(boolVarNestName), env);

    if (Value::Type::Bool != pValue->type) {
        AWESOME_THROW(BooleanException) << "A value other than bool type can not be described in BooleanScope by itself.";
    }
    return { pValue, isDenial };
}

std::tuple<RefOrEntityValue, RefOrEntityValue> parseCompareTargetValues(
    Enviroment const& env,
    Line const& line,
    size_t compareOpStart,
    Value const* pUsedLeftValue,
    Value const* pUsedRightValue)
{
    RefOrEntityValue leftValue(pUsedLeftValue);
    if (nullptr == pUsedLeftValue) {
        auto leftValueLine = Line(line.get(0), 0, compareOpStart - 1);
        leftValue = getValue(env, leftValueLine);
    }

    auto startRightValue = line.incrementPos(compareOpStart, [](auto line, auto pos) {
        return !isSpace(line.get(pos));
    });
    startRightValue = line.skipSpace(startRightValue);

    RefOrEntityValue rightValue = pUsedRightValue;
    if (nullptr == pUsedRightValue) {
        auto rightValueLine = Line(line.get(startRightValue), 0, line.length() - startRightValue);
        rightValue = getValue(env, rightValueLine);
    }
    return { leftValue, rightValue };
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

bool compareValues(Value const& left, Value const& right, CompareOperator compareOp)
{
    bool isResult = false;
    switch (compareOp) {
    case CompareOperator::Equal:        isResult = (left == right); break;
    case CompareOperator::NotEqual:     isResult = (left != right); break;
    case CompareOperator::Greater:      isResult = (left > right); break;
    case CompareOperator::GreaterEqual: isResult = (left >= right); break;
    case CompareOperator::Less:         isResult = (left < right); break;
    case CompareOperator::LessEqual:    isResult = (left <= right); break;
    default:
        AWESOME_THROW(BooleanException) << "use unknown compare...";
    }
    return isResult;
}

std::tuple<CompareOperator, EndPos> parseCompareOperator(Line& line, size_t start)
{
    start = line.skipSpace(start);
    auto p = line.incrementPos(start, [](auto line, auto p) { return !isSpace(line.get(p)); });
    auto opType = toCompareOperatorType(line.substr(start, p - start));
    return { opType, p};
}

std::tuple<LogicOperator, EndPos> findLogicOperator(Line const& line, size_t start)
{
    auto const isNotSeparator = [](auto line, auto pos) {
        return !isSpace(line.get(pos)) && ',' != *line.get(pos);
    };

    auto pos = line.skipSpace(start);
    // search logical operator beginning with '\'
    while (!line.isEndLine(pos)) {
        auto backSlashPos = line.incrementPos(pos, [](auto line, auto pos) {
            return '\\' != *line.get(pos);
        });

        auto wordEnd = line.incrementPos(backSlashPos, isNotSeparator);
        if (wordEnd == backSlashPos) {
            wordEnd = std::min(wordEnd + 1, line.length());
        }
        auto keyward = line.substr(backSlashPos + 1, wordEnd - backSlashPos);
        auto logicOp = toLogicOperatorType(keyward);
        if (LogicOperator::Unknown != logicOp) {
            return { logicOp, wordEnd };
        }
        pos = wordEnd;
        if (',' != *line.get(wordEnd))
            ++pos;
    }

    // search logical operator.
    pos = line.skipSpace(start);
    while (!line.isEndLine(pos)) {
        auto wordEnd = line.incrementPos(pos, isNotSeparator);

        if (wordEnd == pos) {
            wordEnd = std::min(wordEnd + 1, line.length());
        }
        auto keyward = line.substr(pos, wordEnd - pos);
        auto logicOp = toLogicOperatorType(keyward);
        if (LogicOperator::Unknown != logicOp) {
            return { logicOp, wordEnd };
        }
        pos = wordEnd;
        if (',' != *line.get(wordEnd))
            ++pos;
    }
    return { LogicOperator::Continue, std::min(pos, line.length()) };
}

void foreachLogicOperator(Line const& line, size_t start, std::function<bool(Line, LogicOperator)> predicate)
{
    auto pos = start;
    while (!line.isEndLine(pos)) {
        auto startPos = line.skipSpace(pos);
        auto[logicOp, logicOpEnd] = findLogicOperator(line, startPos);
        auto logicStr = toString(logicOp);
        size_t logicOpStart = logicOpEnd;
        switch (logicOp) {
        case LogicOperator::And: [[fallthrough]];
        case LogicOperator::Or:
            logicOpStart = logicOpEnd - logicStr.length();
            break;
        default: [[fallthrough]];
        case LogicOperator::Continue:
            if (logicStr == boost::string_view(line.get(logicOpEnd - logicStr.size()), logicStr.size())) {
                logicOpStart = logicOpEnd - 1;
            }
            break;
        }
        auto booleanLine = Line(line.get(startPos), 0, logicOpStart - startPos);

        if (!predicate(booleanLine, logicOp)) {
            break;
        }
        pos = logicOpEnd;
    }
}

std::tuple<CompareOperator, EndPos> findCompareOperator(Line const& line, size_t start)
{
    auto pos = line.skipSpace(start);
    // search logical operator beginning with '\'
    while (!line.isEndLine(pos)) {
        auto backSlashPos = line.incrementPos(pos, [](auto line, auto pos) {
            return '\\' != *line.get(pos);
        });
        if (backSlashPos == line.length()) {
            break;
        }
        auto wordEnd = line.incrementPos(backSlashPos, [](auto line, auto pos) {
            return !isSpace(line.get(pos));
        });
        auto keyward = line.substr(backSlashPos + 1, wordEnd - backSlashPos);
        auto compareOp = toCompareOperatorType(keyward);
        if (CompareOperator::Unknown != compareOp) {
            return { compareOp, wordEnd };
        }
        pos = wordEnd + 1;
    }

    // search logical operator.
    pos = line.skipSpace(start);
    while (!line.isEndLine(pos)) {
        auto wordEnd = line.incrementPos(pos, [](auto line, auto pos) {
            return !isSpace(line.get(pos));
        });
        if (wordEnd == line.length()) {
            break;
        }
        auto keyward = line.substr(pos, wordEnd - pos);
        auto compareOp = toCompareOperatorType(keyward);
        if (CompareOperator::Unknown != compareOp) {
            return { compareOp, wordEnd };
        }
        pos = wordEnd + 1;
    }
    return { CompareOperator::Unknown, std::min(pos, line.length()) };
}

std::tuple<bool, EndPos> doExistDenialKeyward(Line const& line)
{
    bool isDenial = false;
    if (5 <= line.length()) {
        if ("not " == line.substr(0, 4)) {
            auto p = line.skipSpace(4);
            return { true, p };
        }
    }
    if (3 <= line.length()) {
        if ("!" == line.substr(0, 1)) {
            isDenial = true;
            auto p = line.skipSpace(1);
            return { true, p };
        }
    }
    return { false, 0 };
}


}