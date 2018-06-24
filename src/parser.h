#pragma once

#include <string>
#include <boost/filesystem.hpp>

namespace watagashi::parser
{

void parse(boost::filesystem::path const& filepath);
void parse(char const* source, std::size_t length);
inline void parse(std::string const& source) {
    parse(source.c_str(), source.size());
}


}