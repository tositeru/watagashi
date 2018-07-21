#pragma once

#include <boost/filesystem.hpp>

namespace parser
{

struct Location
{
    boost::filesystem::path filepath;
    size_t row;

    Location();
    Location(boost::filesystem::path const& filepath, size_t row);

    bool empty()const;
};

}