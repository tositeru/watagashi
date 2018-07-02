#pragma once

#include <list>
#include <string>

#include <boost/utility/string_view.hpp>
#include <boost/optional.hpp>

#include "parserUtility.h"

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
        Next,
        Redo,
    };
public:
    virtual ~IParseMode() {}

    virtual Result parse(Enviroment& parser, Line& line) = 0;
};

ErrorHandle evalIndent(Enviroment& env, Line& line, int& outLevel);
CommentType evalComment(Enviroment& env, Line& line);
ErrorHandle closeTopScope(Enviroment& env);
boost::optional<boost::string_view> pickupName(Line const& line, size_t start);
std::list<boost::string_view> parseName(size_t& tailPos, Line const& line, size_t start);
OperatorType parseOperator(size_t& outTailPos, Line const& line, size_t start);
ErrorHandle searchValue(Value** ppOut, std::list<std::string> const& nestName, Enviroment& env, bool doGetParent = false);
ErrorHandle parseValue(Enviroment& env, Line& valueLine);
size_t parseArrayElement(Enviroment& env, Line& line, size_t start);
ErrorHandle parseObjectName(boost::string_view& out, size_t& outTail, Enviroment& env, Line& line, size_t start);

}