#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include <boost/variant.hpp>
#include <boost/utility/string_view.hpp>

#include "../exception.hpp"

namespace parser
{

struct Enviroment;
class ErrorHandle;
class IScope;

struct NoneValue
{};

struct MemberDefined;
struct ObjectDefined
{
    std::string name;
    std::unordered_map<std::string, MemberDefined> members;

    ObjectDefined();
    ObjectDefined(std::string const& name);
};

struct Value;
struct Object
{
    std::unordered_map<std::string, Value> members;
    ObjectDefined const* pDefined;

    Object(ObjectDefined const* pDefined);

    bool applyObjectDefined();

};

struct Reference
{
    Enviroment const* pEnv;
    std::list<std::string> nestName;
    Reference(Enviroment const* pEnv, std::list<std::string> const& nestName);
    Value const* ref()const;
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
        MemberDefined,
        Reference,
    };

    using string = std::string;
    using number = double;
    using array = std::vector<Value>;
    using object = Object;

    static Value const none;
    static Value const emptyStr;
    static ObjectDefined const arrayDefined; // dummy objectDefined
    static ObjectDefined const objectDefined;
    static boost::string_view toString(Type type);
    static Type toType(boost::string_view const& str);

    Type type;
    struct InnerData;
    std::unique_ptr<InnerData> pData;

    Value();
    Value(Value const& right);
    Value(Value &&right);
    Value& operator=(Value const& right);
    Value& operator=(Value &&right);
    ~Value();

    template<typename T>
    Value(T const& right)
        : Value()
    {
        *this = right;
    }

    //template<typename T>
    //Value(T && right)
    //    : Value()
    //{
    //    *this = std::move(right);
    //}

    Value& init(Type type_);

    Value& operator=(NoneValue const& right);
    Value& operator=(string const& right);
    Value& operator=(number const& right);
    Value& operator=(array const& right);
    Value& operator=(object const& right);
    Value& operator=(ObjectDefined const& right);
    Value& operator=(MemberDefined const& right);
    Value& operator=(Reference const& right);

    Value& operator=(NoneValue && right);
    Value& operator=(string && right);
    Value& operator=(number && right);
    Value& operator=(array && right);
    Value& operator=(object && right);
    Value& operator=(ObjectDefined && right);
    Value& operator=(MemberDefined && right);
    Value& operator=(Reference && right);

    void pushValue(Value const& pushValue);
    bool addMember(IScope const& member);
    bool addMember(std::string const&name, Value const& value);
    void appendStr(boost::string_view const& strView);

    std::string toString()const;

    template<typename T> T& get()
    {
        return boost::get<T>(this->pData->data);
    }

    template<typename T> T const& get()const {
        return boost::get<T>(this->pData->data);
    }

    bool isExsitChild(std::string const& name)const;

    Value& getChild(std::string const& name);
    Value const& getChild(std::string const& name)const;
};

struct MemberDefined
{
    Value::Type type;
    Value defaultValue;

    MemberDefined() = default;
    MemberDefined(Value::Type type, Value const& defaultValue);
    MemberDefined(Value::Type type, Value && defaultValue);
};

struct Value::InnerData
{
    boost::variant<
        NoneValue,
        string,
        number,
        array,
        object,
        ObjectDefined,
        MemberDefined,
        Reference> data;
};

}