#include "indent.h"

namespace parser
{

int calLevel(boost::string_view const& unitIndent, boost::string_view const& indent)
{
    // !!! Attention Precondition !!!
    // for(auto& u : unitIndent) { 
    //   assert(isSpace(u));
    // }
    // for(auto& u : indent) { 
    //   assert(isSpace(u));
    // }

    if (unitIndent.empty()) {
        return indent.empty() ? 0 : 1;
    }

    if (0 != (indent.size() % unitIndent.size())) {
        return -1;
    }

    int level = 0;
    auto const unitSize = sizeof(unitIndent[0]) * unitIndent.size();
    for (size_t i = 0u; i < indent.size(); i += unitIndent.size()) {
        if (0 != memcmp(&indent[i], &unitIndent[0], unitSize)) {
            return -1;
        }
        ++level;
    }
    return level;
}


Indent::Indent()
    : mCurrentLevel(0)
{}

void Indent::setUnit(boost::string_view const& indent)
{
    Indent tmp;
    tmp.mUnitOfIndent = indent.to_string();
    tmp.mCurrentLevel = indent.empty() ? 0 : 1;
    *this = std::move(tmp);
}

void Indent::setLevel(int level)
{
    this->mCurrentLevel = level;
}

int Indent::calLevel(boost::string_view const& indent)const
{
    auto level = parser::calLevel(this->mUnitOfIndent, indent);
    return level;
}

size_t Indent::unitOfIndentLength()const
{
    return this->mUnitOfIndent.size();
}

int Indent::currentLevel()const {
    return this->mCurrentLevel;
}


}