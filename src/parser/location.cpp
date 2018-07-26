#include "location.h"

namespace parser
{

//----------------------------------------------------------------------------------
//
//  class Location
//
//----------------------------------------------------------------------------------
Location::Location()
    : filepath()
    , row(0)
{}

Location::Location(boost::filesystem::path const& filepath, size_t row)
    : filepath(filepath)
    , row(row)
{}

bool Location::empty()const
{
    return this->filepath.empty() && this->row == 0;
}

}