#pragma once

#include <string>
#include <boost/filesystem.hpp>

#include "value.h"

namespace parser
{

struct ParserDesc
{
    Value externObj = Object(&Value::emptyObjectDefined);
};

Value parse(boost::filesystem::path const& filepath, ParserDesc const& desc);
Value parse(char const* source, std::size_t length, ParserDesc const& desc);
inline Value parse(std::string const& source, ParserDesc const& desc) {
    return parse(source.c_str(), source.size(), desc);
}

void confirmValueInInteractive(Value const& value);

}