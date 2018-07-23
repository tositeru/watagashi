#pragma once

#include <string>
#include <boost/utility/string_view.hpp>

namespace parser
{

int calLevel(boost::string_view const& unitIndent, boost::string_view const& indent);

class Indent
{
    Indent(Indent const&) = delete;
    Indent operator=(Indent const&) = delete;

public:
    Indent();
    Indent(Indent &&) = default;
    Indent& operator=(Indent &&) = default;

    void setUnit(boost::string_view const& indent);
    void setLevel(int level);
    int calLevel(boost::string_view const& indent)const;

    size_t unitOfIndentLength()const;

public:
    int currentLevel()const;

private:
    std::string mUnitOfIndent;
    int mCurrentLevel;
};

}