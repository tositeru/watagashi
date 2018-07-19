#pragma once

#include <list>
#include <string>
#include <tuple>
#include <boost/utility/string_view.hpp>
#include <boost/optional.hpp>

#include "parserUtility.h"
#include "value.h"

namespace parser
{

struct Enviroment;
class Line;
struct Value;

class IParseMode
{
public:
    enum class Result
    {
        Continue,
        Redo,
        NextLine,
    };

public:
    virtual ~IParseMode() {}

    virtual Result parse(Enviroment& env, Line& line) = 0;
    virtual Result preprocess(Enviroment& env, Line& line);
};

using StartPos = size_t;
using EndPos = StartPos;

int evalIndent(Enviroment& env, Line& line);
CommentType evalComment(Enviroment& env, Line& line);

OperatorType parseOperator(size_t& outEndPos, Line const& line, size_t start);
MemberDefinedOperatorType parseMemberDefinedOperator(size_t& outEndPos, Line const& line, size_t start);

std::tuple<LogicOperator, EndPos> findLogicOperator(Line const& line, size_t start);
void foreachLogicOperator(Line const& line, size_t start, std::function<bool(Line, LogicOperator)> predicate);

bool compareValues(Value const& left, Value const& right, CompareOperator compareOp);
std::tuple<CompareOperator, EndPos> parseCompareOperator(Line& line, size_t start);
std::tuple<CompareOperator, EndPos> findCompareOperator(Line const& line, size_t start);
std::tuple<bool, EndPos> doExistDenialKeyward(Line const& line);

std::tuple<boost::string_view, bool> pickupName(Line const& line, size_t start);
std::tuple<std::list<boost::string_view>, EndPos> parseName(Line const& line, size_t start);
std::tuple<std::list<boost::string_view>, EndPos> parseName(Line const& line, size_t start, bool &outIsSuccess);
std::tuple<std::list<boost::string_view>, EndPos> parseObjectName(Enviroment const& env, Line& line, size_t start);

std::list<std::string> convertToAbsolutionNestName(std::list<std::string> const& nestName, Enviroment const& env);

Value* searchValue(std::list<std::string> const& nestName, Enviroment & env, bool doGetParent = false);
Value const* searchValue(std::list<std::string> const& nestName, Enviroment const& env, bool doGetParent = false);
Value* searchValue(bool& outIsSuccess, std::list<std::string> const& nestName, Enviroment & env, bool doGetParent = false);
Value const* searchValue(bool& outIsSuccess, std::list<std::string> const& nestName, Enviroment const& env, bool doGetParent = false);
RefOrEntityValue getValue(Enviroment const& env, Line& valueLine);

ObjectDefined const* searchObjdectDefined(std::list<boost::string_view> const& nestName, Enviroment const& env);

void parseValue(Enviroment& env, Line& valueLine);
Value parseValueInSingleLine(Enviroment const& env, Line& valueLine);
std::tuple<StartPos, EndPos> searchArraySeparaterPos(Line const& line, size_t start);

size_t parseArrayElement(Enviroment& env, Line& line, size_t start);
EndPos foreachArrayElement(Line const& line, size_t start, std::function<bool(Line const&)> predicate);
Value::Type parseValueType(Enviroment& env, Line& line, size_t& inOutPos);

std::tuple<Value const*, bool> parseBool(Enviroment const& env, Line const& line);

std::tuple<RefOrEntityValue, RefOrEntityValue> parseCompareTargetValues(Enviroment const& env, Line const& line, size_t compareOpStart, Value const* pUsedLeftValue, Value const* pUsedRightValue);

std::string expandVariable(std::string & inOutStr, Enviroment const& env);

}