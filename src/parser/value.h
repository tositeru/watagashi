#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include <boost/variant.hpp>
#include <boost/utility/string_view.hpp>

namespace parser
{

class ErrorHandle;
class IScope;

struct NoneValue
{};

struct MemberDefined;
struct ObjectDefined
{
    std::unordered_map<std::string, MemberDefined> members;
};

struct Value;
struct Object
{
    std::unordered_map<std::string, Value> members;
    ObjectDefined const* pDefined;

    Object(ObjectDefined const* pDefined);
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
    using object = Object;

    static Value const none;
    static ObjectDefined const baseObject;
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
    Value(Value const&) = default;
    Value(Value &&) = default;
    Value& init(Type type_);

    Value& operator=(Value const&);
    Value& operator=(NoneValue const& right);
    Value& operator=(string const& right);
    Value& operator=(number const& right);
    Value& operator=(array const& right);
    Value& operator=(object const& right);
    Value& operator=(ObjectDefined const& right);

    Value& operator=(Value &&);
    Value& operator=(NoneValue && right);
    Value& operator=(string && right);
    Value& operator=(number && right);
    Value& operator=(array && right);
    Value& operator=(object && right);
    Value& operator=(ObjectDefined && right);

    void pushValue(Value const& pushValue);
    bool addMember(IScope const& member);
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

struct MemberDefined
{
    Value::Type type;
    Value defaultValue;
};

}