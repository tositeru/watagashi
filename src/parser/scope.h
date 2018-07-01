#pragma once

#include <string>

#include <boost/utility/string_view.hpp>

#include "value.h"
#include "parserUtility.h"

namespace parser
{

class IScope
{
public:
    enum class Type
    {
        Normal,
        Reference,
    };

public:
    virtual ~IScope() {}

    virtual Type type()const = 0;
    virtual std::list<std::string> const& nestName()const = 0;
    virtual Value& value() = 0;
    virtual Value const& value()const = 0;
    virtual Value::Type valueType()const = 0;
};

class NormalScope : public IScope
{
    std::list<std::string> mNestName;
    Value mValue;

public:
    NormalScope()=default;
    NormalScope(std::list<std::string> const& nestName, Value const& value)
        : mNestName(nestName)
        , mValue(value)
    {}
    NormalScope(std::list<std::string> && nestName, Value && value)
        : mNestName(std::move(nestName))
        , mValue(std::move(value))
    {}
    NormalScope(std::list<boost::string_view> const& nestName, Value const& value)
        : mNestName()
        , mValue(value)
    {
        this->mNestName = toStringList(nestName);
    }

    Type type()const override{ return Type::Normal; }

    std::list<std::string> const& nestName()const override
    {
        return this->mNestName;
    }

    Value& value() override
    {
        return this->mValue;
    }

    Value const& value()const override
    {
        return this->mValue;
    }

    Value::Type valueType()const override
    {
        return this->mValue.type;
    }
};

class ReferenceScope : public IScope
{
    std::list<std::string> mNestName;
    Value& mRefValue;
public:
    ReferenceScope(std::list<std::string> const& nestName, Value& value)
        : mNestName(nestName)
        , mRefValue(value)
    {}
    ReferenceScope(std::list<boost::string_view> const& nestName, Value & value)
        : mNestName()
        , mRefValue(value)
    {
        this->mNestName = toStringList(nestName);
    }

    Type type()const override { return Type::Reference; }

    std::list<std::string> const& nestName()const override
    {
        return this->mNestName;
    }

    Value& value() override
    {
        return this->mRefValue;
    }

    Value const& value()const override
    {
        return this->mRefValue;
    }

    Value::Type valueType()const override
    {
        return this->value().type;
    }
};

}