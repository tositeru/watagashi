#include "value.h"

#include <boost/bimap.hpp>
#include <boost/assign.hpp>

#include "../exception.hpp"
#include "parserUtility.h"
#include "scope.h"

namespace parser
{
//-----------------------------------------------------------------------
//
//  struct Object
//
//-----------------------------------------------------------------------
Object::Object(ObjectDefined const* pDefined)
    : pDefined(pDefined)
{
}

ErrorHandle Object::applyObjectDefined()
{
    MakeErrorHandle error(0);
    for (auto&& [name, memberDefined] : this->pDefined->members) {
        auto it = this->members.find(name);
        if (this->members.end() != it) {
            continue;
        }
        if (Value::Type::None == memberDefined.defaultValue.type) {
            error << " define object error: must set value to '" << name << "'.\n";
        } else {
            this->members.insert({name, memberDefined.defaultValue });
        }
    }
    return error;
}

//-----------------------------------------------------------------------
//
//  struct Value
//
//-----------------------------------------------------------------------
Value const Value::none = Value().init(Value::Type::None);
ObjectDefined const Value::arrayDefined;
ObjectDefined const Value::baseObject;

Value::Value()
    : type(Type::None)
    , pData(std::make_unique<InnerData>())
{
    this->pData->data = NoneValue();
}

Value::~Value()
{}

Value::Value(Value const& right)
    : type(right.type)
    , pData(std::make_unique<InnerData>(*right.pData))
{}

Value::Value(Value && right)
    : type(right.type)
    , pData(std::move(right.pData))
{}

Value& Value::operator=(Value const& right)
{
    Value tmp(right);
    *this = std::move(tmp);
    return *this;
}

Value& Value::operator=(Value &&right)
{
    this->type = right.type;
    this->pData = std::move(right.pData);
    return *this;
}

Value& Value::init(Type type_)
{
    switch (type_) {
    case Type::None:   this->pData->data = NoneValue(); break;
    case Type::String: this->pData->data = ""; break;
    case Type::Number: this->pData->data = 0.0; break;
    case Type::Array:  this->pData->data = array{}; break;
    case Type::Object: this->pData->data = object(&Value::baseObject); break;
    case Type::ObjectDefined: this->pData->data = ObjectDefined{}; break;
    case Type::MemberDefined: this->pData->data = MemberDefined{}; break;
    }
    this->type = type_;
    return *this;
}

Value& Value::operator=(NoneValue const& right)
{
    this->pData->data = right;
    this->type = Type::None;
    return *this;
}

Value& Value::operator=(string const& right)
{
    this->pData->data = right;
    this->type = Type::String;
    return *this;
}

Value& Value::operator=(number const& right)
{
    this->pData->data = right;
    this->type = Type::Number;
    return *this;
}

Value& Value::operator=(array const& right)
{
    this->pData->data = right;
    this->type = Type::Array;
    return *this;
}

Value& Value::operator=(object const& right)
{
    this->pData->data = right;
    this->type = Type::Object;
    return *this;
}

Value& Value::operator=(ObjectDefined const& right)
{
    this->pData->data = right;
    this->type = Type::ObjectDefined;
    return *this;
}

Value& Value::operator=(MemberDefined const& right)
{
    this->pData->data = right;
    this->type = Type::MemberDefined;
    return *this;
}

Value& Value::operator=(NoneValue && right)
{
    this->pData->data = std::move(right);
    this->type = Type::None;
    return *this;
}

Value& Value::operator=(string && right)
{
    this->pData->data = std::move(right);
    this->type = Type::String;
    return *this;
}

Value& Value::operator=(number && right)
{
    this->pData->data = std::move(right);
    this->type = Type::Number;
    return *this;
}

Value& Value::operator=(array && right)
{
    this->pData->data = std::move(right);
    this->type = Type::Array;
    return *this;
}

Value& Value::operator=(object && right)
{
    this->pData->data = std::move(right);
    this->type = Type::Object;
    return *this;
}

Value& Value::operator=(ObjectDefined && right)
{
    this->pData->data = std::move(right);
    this->type = Type::ObjectDefined;
    return *this;
}

Value& Value::operator=(MemberDefined && right)
{
    this->pData->data = std::move(right);
    this->type = Type::MemberDefined;
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
    boost::apply_visitor(PushValue(pushValue), this->pData->data);
}

class AddMember : public boost::static_visitor<bool>
{
    IScope const& mMember;
public:
    AddMember(IScope const& member)
        : mMember(member)
    {}

    template<typename T>
    bool operator()(T&)const
    {
        AWESOME_THROW(std::runtime_error)
            << "invalid operation. this value don't add a member. Only object enable to add a member.";
        return false;
    }

    template<>
    bool operator()(Value::array& arr)const
    {
        auto& name = this->mMember.nestName().back();
        bool isNumber = false;
        auto index = static_cast<int>(toDouble(name, isNumber));
        if (isNumber && 0 <= index && index < arr.size()) {
            arr[index] = this->mMember.value();
            return true;
        }
        return false;
    }

    template<>
    bool operator()(Value::object& obj)const
    {
        auto& objName = this->mMember.nestName().back();
        auto it = obj.members.find(objName);
        if (obj.members.end() == it) {
            obj.members.insert({ objName, this->mMember.value() });
        } else {
            it->second = this->mMember.value();
        }
        return true;
    }

    template<>
    bool operator()(ObjectDefined& obj)const
    {
        auto& objName = this->mMember.nestName().back();
        auto it = obj.members.find(objName);
        if (obj.members.end() == it) {
            auto defined = this->mMember.value().get<MemberDefined>();
            obj.members.insert({ objName,  defined });
        } else {
            it->second = this->mMember.value().get<MemberDefined>();
        }
        return true;
    }

};

bool Value::addMember(IScope const& member)
{
    return boost::apply_visitor(AddMember(member), this->pData->data);
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
    boost::apply_visitor(AppendString(strView), this->pData->data);
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

Value::Type Value::toType(boost::string_view const& str)
{
    auto it = valueTypeBimap.left.find(str);
    return valueTypeBimap.left.end() == it
        ? Type::None
        : it->get_right();
}

class ToString : public boost::static_visitor<std::string>
{
public:
    std::string operator()(NoneValue const&)const
    {
        return "[None]";
    }

    std::string operator()(Value::string const& str)const
    {
        return str;
    }

    std::string operator()(Value::number const& num)const
    {
        return std::to_string(num);
    }

    std::string operator()(Value::array const& arr)const
    {
        return "[Array](" + std::to_string(arr.size()) + ")";
    }

    std::string operator()(Value::object const& obj)const
    {
        return "[Object](" + std::to_string(obj.members.size()) + ")";
    }

    std::string operator()(ObjectDefined const& objDefined)const
    {
        return "[ObjectDefined](" + std::to_string(objDefined.members.size()) + ")";
    }

    std::string operator()(MemberDefined const& memberDefined)const
    {
        return "[ObjectDefined](" + Value::toString(memberDefined.type).to_string() + ")";
    }

};

std::string Value::toString()const
{
    return boost::apply_visitor(ToString(), this->pData->data);
}

bool Value::isExsitChild(std::string const& name)const
{
    switch (this->type) {
    case Value::Type::Object:
    {
        auto& obj = this->get<Value::object>();
        auto it = obj.members.find(name);
        return obj.members.end() != it;
    }
    case Value::Type::Array:
    {
        bool isNumber = false;
        auto index = static_cast<int>(toDouble(name, isNumber));
        if (!isNumber) {
            return false;
        }
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
        auto it = obj.members.find(name);
        if (obj.members.end() == it) {
            error = MakeErrorHandle(0)
                <<"syntax error!! Don't found '" << name << "' child.";
            break;
        }
        return it->second;
    }
    case Value::Type::Array:
    {

        bool isNumber = false;
        auto index = static_cast<int>(toDouble(name, isNumber));
        if (!isNumber) {
            error = MakeErrorHandle(0)
                << "syntax error!! Use index(" << index << ") outside the array range. (array size=" << this->get<Value::array>().size() << ")";
            return Value::none;
        }
        auto& arr = this->get<Value::array>();
        if (arr.size() <= index) {
            error = MakeErrorHandle(0)
                << "syntax error!! Use index(" << index << ") outside the array range. (array size=" << this->get<Value::array>().size() << ")";
        }
        return arr[index];
    }
    default: break;
    }
    return Value::none;
}

}