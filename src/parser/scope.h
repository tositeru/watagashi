#pragma once

#include <string>

#include <boost/utility/string_view.hpp>

#include "value.h"

namespace parser
{

struct Scope
{
    std::list<std::string> nestName;
    Value value;

    Scope()=default;
    Scope(std::list<std::string> const& nestName, Value const& value)
        : nestName(nestName)
        , value(value)
    {}
    Scope(std::list<std::string> && nestName, Value && value)
        : nestName(std::move(nestName))
        , value(std::move(value))
    {}
    Scope(std::list<boost::string_view> const& nestName, Value const& value)
        : nestName()
        , value(value)
    {
        for (auto& view : nestName) {
            this->nestName.push_back(view.to_string());
        }
    }

    Value::Type valueType()const
    {
        return this->value.type;
    }
};

}