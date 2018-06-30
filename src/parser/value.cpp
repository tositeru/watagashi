#include "value.h"

#include <boost/bimap.hpp>
#include <boost/assign.hpp>

#include "../exception.hpp"
#include "parserUtility.h"
#include "scope.h"

namespace parser
{

Value const Value::none = Value().init(Value::Type::None);

Value& Value::init(Type type_)
{
    switch (type_) {
    case Type::None:   this->data = Value::none.data; break;
    case Type::String: this->data = ""; break;
    case Type::Number: this->data = 0.0; break;
    case Type::Array:  this->data = array{}; break;
    case Type::Object: this->data = object{}; break;
    case Type::ObjectDefined: this->data = ObjectDefined{}; break;
    }
    this->type = type_;
    return *this;
}

class PushValue : public boost::static_visitor<void>
{
    Value const& mPushValue;
public:
    PushValue(Value const& pushValue)
        : mPushValue(pushValue)
    {}

    template<typename T>
    void operator()(T& )const
    {
        AWESOME_THROW(std::runtime_error)
            << "invalid operation. this value don't push a Value. Only array enable to push a Value.";
    }

    template<>
    void operator()(Value::array& arr)const
    {
        arr.push_back(this->mPushValue);
    }

};

void Value::pushValue(Value const& pushValue)
{
    boost::apply_visitor(PushValue(pushValue), this->data);
}

class AddMember : public boost::static_visitor<void>
{
    Scope const& mMember;
public:
    AddMember(Scope const& member)
        : mMember(member)
    {}

    template<typename T>
    void operator()(T&)const
    {
        AWESOME_THROW(std::runtime_error)
            << "invalid operation. this value don't add a member. Only object enable to add a member.";
    }

    template<>
    void operator()(Value::object& obj)const
    {
        auto& objName = this->mMember.nestName.back();
        auto it = obj.find(objName);
        if (obj.end() == it) {
            obj.insert({ objName, this->mMember.value });
        } else {
            it->second = this->mMember.value;
        }
    }

};

void Value::addMember(Scope const& member)
{
    boost::apply_visitor(AddMember(member), this->data);
}

class AppendString : public boost::static_visitor<void>
{
    boost::string_view const& mStringView;
public:
    AppendString(boost::string_view const& mStringView)
        : mStringView(mStringView)
    {}

    template<typename T>
    void operator()(T&)const
    {
        AWESOME_THROW(std::runtime_error)
            << "invalid operation. this value don't append a string. Only string enable to append a string.";
    }

    template<>
    void operator()(std::string& str)const
    {
        str += "\n" + this->mStringView.to_string();
    }

};

void Value::appendStr(boost::string_view const& strView)
{
    boost::apply_visitor(AppendString(strView), this->data);
}

using ValueTypeBimap = boost::bimap<boost::string_view, Value::Type>;
static const ValueTypeBimap valueTypeBimap = boost::assign::list_of<ValueTypeBimap::relation>
    ("none", Value::Type::None)
    ("string", Value::Type::String)
    ("number", Value::Type::Number)
    ("array", Value::Type::Array)
    ("object", Value::Type::Object)
    ("objectDefined", Value::Type::ObjectDefined);

boost::string_view Value::toString(Type type)
{
    static char const* UNKNOWN = "(unknown)";
    auto it = valueTypeBimap.right.find(type);
    return valueTypeBimap.right.end() == it
        ? UNKNOWN
        : it->get_left();
}

bool Value::isExsitChild(std::string const& name)const
{
    switch (this->type) {
    case Value::Type::Object:
    {
        auto& obj = this->get<Value::object>();
        auto it = obj.find(name);
        return obj.end() != it;
    }
    case Value::Type::Array:
    {
        int index = std::stoi(name);
        auto& arr = this->get<Value::array>();
        return 0 <= index && index < arr.size();
    }
    default:
        break;
    }
    return false;
}

Value& Value::getChild(std::string const& name, ErrorHandle& error)
{
    auto const* constThis = this;
    return const_cast<Value&>(constThis->getChild(name, error));
}

Value const& Value::getChild(std::string const& name, ErrorHandle& error)const
{
    switch (this->type) {
    case Value::Type::Object:
    {
        auto& obj = this->get<Value::object>();
        auto it = obj.find(name);
        if (obj.end() == it) {
            error = MakeErrorHandle(0)
                <<"syntax error!! Don't found '" << name << "' child.";
            break;
        }
        return it->second;
    }
    case Value::Type::Array:
    {
        int index = -1;
        try {
            index = std::stoi(name);
            auto& arr = this->get<Value::array>();
            if (arr.size() <= index) {
                throw std::invalid_argument("");
            }
            return arr[index];
        } catch (std::invalid_argument& e) {
            error = MakeErrorHandle(0)
                << "syntax error!! Use index(" << index << ") outside the array range. (array size=" << this->get<Value::array>().size() << ")";
        }
    }
    default: break;
    }
    return Value::none;
}

}