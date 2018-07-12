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

int evalIndent(Enviroment& env, Line& line);
CommentType evalComment(Enviroment& env, Line& line);
void closeTopScope(Enviroment& env);
std::tuple<boost::string_view, bool> pickupName(Line const& line, size_t start);
std::tuple<std::list<boost::string_view>, size_t> parseName(Line const& line, size_t start);
std::tuple<std::list<boost::string_view>, size_t> parseName(Line const& line, size_t start, bool &outIsSuccess);
OperatorType parseOperator(size_t& outEndPos, Line const& line, size_t start);
Value* searchValue(std::list<std::string> const& nestName, Enviroment & env, bool doGetParent = false);
Value const* searchValue(std::list<std::string> const& nestName, Enviroment const& env, bool doGetParent = false);
Value* searchValue(bool& outIsSuccess, std::list<std::string> const& nestName, Enviroment & env, bool doGetParent = false);
Value const* searchValue(bool& outIsSuccess, std::list<std::string> const& nestName, Enviroment const& env, bool doGetParent = false);
void parseValue(Enviroment& env, Line& valueLine);
Value parseValueInSingleLine(Enviroment const& env, Line& valueLine);
size_t parseArrayElement(Enviroment& env, Line& line, size_t start);
std::tuple<std::list<boost::string_view>, size_t> parseObjectName(Enviroment const& env, Line& line, size_t start);
Value::Type parseValueType(Enviroment& env, Line& line, size_t& inOutPos);
MemberDefinedOperatorType parseMemberDefinedOperator(size_t& outEndPos, Line const& line, size_t start);
ObjectDefined const* searchObjdectDefined(std::list<boost::string_view> const& nestName, Enviroment const& env);
std::string expandVariable(std::string & inOutStr, Enviroment const& env);
std::tuple<CompareOperator, size_t> parseCompareOperator(Line& line, size_t start);

}