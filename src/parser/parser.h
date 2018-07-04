#pragma once

#include <string>
#include <boost/filesystem.hpp>

#include "value.h"

namespace parser
{

Value parse(boost::filesystem::path const& filepath);
Value parse(char const* source, std::size_t length);
inline Value parse(std::string const& source) {
    return parse(source.c_str(), source.size());
}

void confirmValueInInteractive(Value const& value);

}