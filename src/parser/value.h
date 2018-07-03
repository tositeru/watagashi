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
        MemberDefined,
    };

    using string = std::string;
    using number = double;
    using array = std::vector<Value>;
    using object = Object;

    static Value const none;
    static ObjectDefined const baseObject;
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

    Value& init(Type type_);

    Value& operator=(NoneValue const& right);
    Value& operator=(string const& right);
    Value& operator=(number const& right);
    Value& operator=(array const& right);
    Value& operator=(object const& right);
    Value& operator=(ObjectDefined const& right);
    Value& operator=(MemberDefined const& right);

    Value& operator=(NoneValue && right);
    Value& operator=(string && right);
    Value& operator=(number && right);
    Value& operator=(array && right);
    Value& operator=(object && right);
    Value& operator=(ObjectDefined && right);
    Value& operator=(MemberDefined && right);

    void pushValue(Value const& pushValue);
    bool addMember(IScope const& member);
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

    Value& getChild(std::string const& name, ErrorHandle& error);
    Value const& getChild(std::string const& name, ErrorHandle& error)const;
};

struct MemberDefined
{
    Value::Type type;
    Value defaultValue;
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
        MemberDefined> data;
};

}