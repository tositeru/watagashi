#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include <boost/variant.hpp>
#include <boost/utility/string_view.hpp>

namespace parser
{

class ErrorHandle;

struct Scope;

struct ObjectDefined
{
    // TODO 
};

struct NoneValue
{
};

struct Value
{
    enum class Type
    {
        None,
        String,
        Number,
        Array,
        Object,
        ObjectDefined,
    };
    using string = std::string;
    using number = double;
    using array = std::vector<Value>;
    using object = std::unordered_map<std::string, Value>;

    static Value const none;
    static boost::string_view toString(Type type);

    Type type;
    boost::variant<
        NoneValue,
        string,
        number,
        array,
        object,
        ObjectDefined> data;

    Value() = default;
    Value& init(Type type_);


    void pushValue(Value const& pushValue);
    bool addMember(Scope const& member);
    void appendStr(boost::string_view const& strView);

    std::string toString()const;

    template<typename T> T& get()
    {
        return boost::get<T>(this->data);
    }

    template<typename T> T const& get()const {
        return boost::get<T>(this->data);
    }

    bool isExsitChild(std::string const& name)const;

    Value& getChild(std::string const& name, ErrorHandle& error);
    Value const& getChild(std::string const& name, ErrorHandle& error)const;
};

}