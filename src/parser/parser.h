#pragma once

#include <string>

#include "value.h"
#include "location.h"
#include "enviroment.h"

namespace parser
{

struct ParserDesc
{
    Value externObj = Object(&Value::emptyObjectDefined);
    Value globalObj = Object(&Value::emptyObjectDefined);
    Location location;
};

struct ParseResult
{
    Value globalObj;
    std::vector<Value> returnValues;
};

ParseResult parse(boost::filesystem::path const& filepath, ParserDesc const& desc);
ParseResult parse(char const* source, std::size_t length, ParserDesc const& desc);
inline ParseResult parse(std::string const& source, ParserDesc const& desc) {
    return parse(source.c_str(), source.size(), desc);
}

void parse(Enviroment& env);

void confirmValueInInteractive(Value const& value);

}