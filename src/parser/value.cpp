#include "value.h"

#include <iostream>
#include <boost/bimap.hpp>
#include <boost/assign.hpp>
#include <boost/range/irange.hpp>

#include "../exception.hpp"
#include "parserUtility.h"
#include "scope.h"
#include "parseMode.h"
#include "enviroment.h"
#include "parser.h"

using namespace std;

namespace parser
{
//-----------------------------------------------------------------------
//
//  struct ObjectDefined
//
//-----------------------------------------------------------------------
ObjectDefined::ObjectDefined()
{}

ObjectDefined::ObjectDefined(std::string const& name)
    : name(name)
{}

//-----------------------------------------------------------------------
//
//  struct Object
//
//-----------------------------------------------------------------------
Object::Object(ObjectDefined const* pDefined)
    : pDefined(pDefined)
{
}

void Object::applyObjectDefined()
{
    for (auto&& [name, memberDefined] : this->pDefined->members) {
        auto memberIt = this->members.find(name);
        if (memberIt == this->members.end()) {
            if (memberDefined.defaultValue.type == Value::Type::None) {
                AWESOME_THROW(DefinedObjectException)
                    << name << "' in '" << (this->pDefined->name.empty() ? "(Anonymous)" : this->pDefined->name)
                    << "' must set value.";
            } else {
                this->members.insert({name, memberDefined.defaultValue });
            }
        } else {
             if (memberDefined.type != memberIt->second.type) {
                 AWESOME_THROW(DefinedObjectException)
                     << "Type of member value and type of setting value is diffrent...\n"
                     << "target: '" << name << "' in '" << (this->pDefined->name.empty() ? "(Anonymous)" : this->pDefined->name) << "'"
                     << " memberType=" << Value::toString(memberDefined.type) << " setting=" << Value::toString(memberIt->second.type);
             }
        }
    }
}

bool Object::isExistMember(std::string const& name)const
{
    return this->members.count(name) >= 1;
}

Value& Object::getMember(std::string const& name)
{
    auto const* constThis = this;
    return const_cast<Value&>(constThis->getMember(name));
}

Value const& Object::getMember(std::string const& name)const
{
    auto it = this->members.find(name);
    if (it == this->members.end()) {
        AWESOME_THROW(std::invalid_argument)
            << "don't found '" << name << "'member...";
    }
    return it->second;
}

//-----------------------------------------------------------------------
//
//  struct MemberDefined
//
//-----------------------------------------------------------------------

MemberDefined::MemberDefined(Value::Type type, Value const& defaultValue)
    : type(type)
    , defaultValue(defaultValue)
{}

MemberDefined::MemberDefined(Value::Type type, Value && defaultValue)
    : type(type)
    , defaultValue(std::move(defaultValue))
{}

//-----------------------------------------------------------------------
//
//  struct Reference
//
//-----------------------------------------------------------------------

Reference::Reference(Enviroment const* pEnv, std::list<std::string> const& nestName)
    : pEnv(pEnv)
    , nestName(nestName)
{
    this->nestName = convertToAbsolutionNestName(nestName, *pEnv);
}

Value const* Reference::ref()const
{
    return this->pEnv->searchValue(this->nestName, false);
}

//-----------------------------------------------------------------------
//
//  struct Function
//
//-----------------------------------------------------------------------
ParseResult Function::execute(std::vector<Value> const& actualArguments)const
{
    ParserDesc parseDesc;
    for (auto&& capture : this->captures) {
        parseDesc.externObj.addMember(capture.name, capture.value);
    }
    // setup arguments
    for (auto index : boost::irange(size_t(0), this->arguments.size())) {
        auto& formalArg = this->arguments[index];
        if (index < actualArguments.size()) {
            auto& actualArg = actualArguments[index];
            if (formalArg.type != Value::Type::None && formalArg.type != actualArg.type) {
                AWESOME_THROW(std::invalid_argument) << "The type between formal and actual argument is different...\n"
                    << "Argument No=" << index+1 << ": formal=" << Value::toString(formalArg.type) << ", actual=" << Value::toString(actualArg.type);
            }
            parseDesc.globalObj.addMember(formalArg.name, actualArg);
        } else {
            // check to exist default value.
            if (formalArg.defaultValue.type == Value::Type::None) {
                AWESOME_THROW(SyntaxException) << "Arguments not was passed...\n"
                    << "Argument No=" << index+1;
            }
            parseDesc.globalObj.addMember(formalArg.name, formalArg.defaultValue);
        }
    }
    // setup variable length arguments
    if (this->arguments.size() < actualArguments.size()) {
        Value variableLengthArgument;
        variableLengthArgument.init(Value::Type::Array);
        for (auto index : boost::irange(this->arguments.size(), actualArguments.size())) {
            auto& actualArg = actualArguments[index];
            variableLengthArgument.pushValue(actualArg);
        }
        parseDesc.globalObj.addMember("__vargs", variableLengthArgument);
    }

    return parse(this->contents, parseDesc);
}

//-----------------------------------------------------------------------
//
//  struct Coroutine
//
//-----------------------------------------------------------------------
Coroutine::Coroutine(Function const* pFunction)
    : pFunction(pFunction)
    , pEnv(std::make_shared<Enviroment>(pFunction->contents))
{
}

void Coroutine::setFunctionArguments(std::vector<Value> && arguments)
{
    auto& function = *this->pFunction;
    // setup arguments
    for (auto index : boost::irange(size_t(0), arguments.size())) {
        auto& formalArgument = this->pFunction->arguments[index];
        auto& entity = arguments[index];
        this->pEnv->globalScope().value().addMember(formalArgument.name, entity);
    }
    // setup arguments by default value
    for (auto index : boost::irange(arguments.size(), this->pFunction->arguments.size())) {
        auto& formalArgument = this->pFunction->arguments[index];
        if (formalArgument.defaultValue.type == Value::Type::None) {
            AWESOME_THROW(SyntaxException)
                << "Arguments of required number not was passed.";
        }
        this->pEnv->globalScope().value().addMember(formalArgument.name, formalArgument.defaultValue);
    }
}

ParseResult Coroutine::execute(std::vector<Value> const& argumentEntitys)
{
    switch (this->pEnv->status) {
    case Enviroment::Status::StandBy:
    {
        for (auto&& capture : this->pFunction->captures) {
            this->pEnv->externObj.addMember(capture.name, capture.value);
        }
        this->pEnv->location = this->pFunction->contentsLocation;
        break;
    }
    case Enviroment::Status::Suspension:
        break;
    default:
        AWESOME_THROW(FatalException) << "This coroutine is completion...";
    }
    this->pEnv->arguments = argumentEntitys;

    parse(*this->pEnv);

    ParseResult result;
    result.returnValues = std::move(this->pEnv->returnValues);
    if (this->pEnv->status == Enviroment::Status::Completion) {
        result.globalObj = std::move(this->pEnv->globalScope().value());
    }
    return result;
}

//-----------------------------------------------------------------------
//
//  struct Value
//
//-----------------------------------------------------------------------
Value const Value::none = Value().init(Value::Type::None);
Value const Value::emptyStr = Value::string();
Value const Value::arrayDefined;
Value const Value::emptyObjectDefined(ObjectDefined("Object"));

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
    , pData(std::make_unique<InnerData>(right.pData->data))
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

Value::Value(NoneValue const& right)
    : type(Type::None)
    , pData(std::make_unique<InnerData>(right))
{}

Value::Value(bool const& right)
    : type(Type::Bool)
    , pData(std::make_unique<InnerData>(right))
{}


Value::Value(string const& right)
    : type(Type::String)
    , pData(std::make_unique<InnerData>(right))
{}

Value::Value(number const& right)
    : type(Type::Number)
    , pData(std::make_unique<InnerData>(right))
{}

Value::Value(array const& right)
    : type(Type::Array)
    , pData(std::make_unique<InnerData>(right))
{}

Value::Value(object const& right)
    : type(Type::Object)
    , pData(std::make_unique<InnerData>(right))
{}

Value::Value(ObjectDefined const& right)
    : type(Type::ObjectDefined)
    , pData(std::make_unique<InnerData>(right))
{}

Value::Value(MemberDefined const& right)
    : type(Type::MemberDefined)
    , pData(std::make_unique<InnerData>(right))
{}

Value::Value(Reference const& right)
    : type(Type::Reference)
    , pData(std::make_unique<InnerData>(right))
{}

Value::Value(Function const& right)
    : type(Type::Function)
    , pData(std::make_unique<InnerData>(right))
{}

Value::Value(Argument const& right)
    : type(Type::Argument)
    , pData(std::make_unique<InnerData>(right))
{}

Value::Value(Capture const& right)
    : type(Type::Capture)
    , pData(std::make_unique<InnerData>(right))
{}

Value::Value(NoneValue && right)
    : type(Type::None)
    , pData(std::make_unique<InnerData>(std::move(right)))
{}

Value::Value(bool && right)
    : type(Type::Bool)
    , pData(std::make_unique<InnerData>(std::move(right)))
{}

Value::Value(string && right)
    : type(Type::String)
    , pData(std::make_unique<InnerData>(std::move(right)))
{}

Value::Value(number && right)
    : type(Type::Number)
    , pData(std::make_unique<InnerData>(std::move(right)))
{}

Value::Value(array && right)
    : type(Type::Array)
    , pData(std::make_unique<InnerData>(std::move(right)))
{}

Value::Value(object && right)
    : type(Type::Object)
    , pData(std::make_unique<InnerData>(std::move(right)))
{}

Value::Value(ObjectDefined && right)
    : type(Type::ObjectDefined)
    , pData(std::make_unique<InnerData>(std::move(right)))
{}

Value::Value(MemberDefined && right)
    : type(Type::MemberDefined)
    , pData(std::make_unique<InnerData>(std::move(right)))
{}

Value::Value(Reference && right)
    : type(Type::Reference)
    , pData(std::make_unique<InnerData>(std::move(right)))
{}

Value::Value(function && right)
    : type(Type::Function)
    , pData(std::make_unique<InnerData>(std::move(right)))
{}

Value::Value(argument && right)
    : type(Type::Argument)
    , pData(std::make_unique<InnerData>(std::move(right)))
{}

Value::Value(Capture && right)
    : type(Type::Capture)
    , pData(std::make_unique<InnerData>(std::move(right)))
{}

Value::Value(Coroutine && right)
    : type(Type::Coroutine)
    , pData(std::make_unique<InnerData>(std::move(right)))
{}

Value& Value::init(Type type_)
{
    switch (type_) {
    case Type::None:   this->pData->data = NoneValue(); break;
    case Type::Bool:   this->pData->data = false; break;
    case Type::String: this->pData->data = ""s; break;
    case Type::Number: this->pData->data = 0.0; break;
    case Type::Array:  this->pData->data = array{}; break;
    case Type::Object: this->pData->data = object(&Value::emptyObjectDefined.get<ObjectDefined>()); break;
    case Type::ObjectDefined: this->pData->data = ObjectDefined{}; break;
    case Type::MemberDefined: this->pData->data = MemberDefined{}; break;
    case Type::Reference: this->pData->data = Reference(nullptr, {""}); break;
    case Type::Function: this->pData->data = Function(); break;
    case Type::Argument: this->pData->data = Argument(); break;
    case Type::Capture: this->pData->data = Capture(); break;
    default:
        AWESOME_THROW(std::invalid_argument) << "unimplement type... type=" << toString(type);
    }
    this->type = type_;
    return *this;
}

bool Value::operator==(Value const& right)const
{
    if (this->type != right.type) {
        AWESOME_THROW(invalid_argument)
            << "The type of the value trying to compare is different... "
            << "(left=" << toString(this->type) << ", right=" << toString(right.type) << ")";
    }

    switch (this->type) {
    case Type::Bool:   return this->get<bool>() == right.get<bool>();
    case Type::String: return this->get<Value::string>() == right.get<Value::string>();
    case Type::Number: return this->get<Value::number>() == right.get<Value::number>();
    default:
        AWESOME_THROW(invalid_argument)
            << "It can not comapre equivalencies except bool, string or number type...";
    }
    return false;
}

bool Value::operator!=(Value const& right)const
{
    return !(*this == right);
}

bool Value::operator<(Value const& right)const
{
    if (this->type != right.type)
        return false;
    switch (this->type) {
    case Type::String: return this->get<Value::string>() < right.get<Value::string>();
    case Type::Number: return this->get<Value::number>() < right.get<Value::number>();
    default:
        AWESOME_THROW(invalid_argument)
            << "It can not comapre except string or number type...";
    }
    return false;
}

bool Value::operator<=(Value const& right)const
{
    if (this->type != right.type)
        return false;
    switch (this->type) {
    case Type::String: return this->get<Value::string>() <= right.get<Value::string>();
    case Type::Number: return this->get<Value::number>() <= right.get<Value::number>();
    default:
        AWESOME_THROW(invalid_argument)
            << "It can not comapre except string or number type...";
    }
    return false;
}

bool Value::operator>(Value const& right)const
{
    return !(*this < right) && *this != right;
}

bool Value::operator>=(Value const& right)const
{
    return !(*this < right) && *this == right;
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
        if (Value::Type::Reference == this->mPushValue.type) {
            auto& ref = this->mPushValue.get<Reference>();
            Value const* pValue = ref.ref();

            if (Value::Type::Array == pValue->type) {
                auto refArr = pValue->get<Value::array>();
                arr.insert(arr.end(), refArr.begin(), refArr.end());
            } else {
                arr.push_back(*pValue);
            }
        } else {
            arr.push_back(this->mPushValue);
        }
    }

};

void Value::pushValue(Value const& pushValue)
{
    boost::apply_visitor(PushValue(pushValue), this->pData->data);
}

class AddMember : public boost::static_visitor<bool>
{
    std::string const& mName;
    Value const& mValue;

public:
    AddMember(IScope const& member)
        : mName(member.nestName().back())
        , mValue(member.value())
    {}
    AddMember(std::string const& name, Value const& value)
        : mName(name)
        , mValue(value)
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
        auto& name = this->mName;
        bool isNumber = false;
        auto index = static_cast<int>(toDouble(name, isNumber));
        if (!(isNumber && 0 <= index && index < arr.size())) {
            return false;
        }
        if (Value::Type::Reference == this->mValue.type) {
            auto& ref = this->mValue.get<Reference>();
            Value const* pValue = ref.ref();
            arr[index] = *pValue;
        } else {
            arr[index] = this->mValue;
        }

        return true;
    }

    template<>
    bool operator()(Value::object& obj)const
    {
        Value const* pValue = &this->mValue;
        if (Value::Type::Reference == this->mValue.type) {
            auto& ref = this->mValue.get<Reference>();
            pValue = ref.ref();
        }

        auto& objName = this->mName;
        auto it = obj.members.find(objName);
        if (obj.members.end() == it) {
            obj.members.insert({ objName, *pValue });
        } else {
            it->second = *pValue;
        }
        return true;
    }

    template<>
    bool operator()(ObjectDefined& obj)const
    {
        auto& objName = this->mName;
        auto it = obj.members.find(objName);
        if (obj.members.end() == it) {
            auto defined = this->mValue.get<MemberDefined>();
            obj.members.insert({ objName,  defined });
        } else {
            it->second = this->mValue.get<MemberDefined>();
        }
        return true;
    }

};

bool Value::addMember(IScope const& member)
{
    return boost::apply_visitor(AddMember(member), this->pData->data);
}

bool Value::addMember(std::string const& name, Value const& value)
{
    return boost::apply_visitor(AddMember(name, value), this->pData->data);
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
        if (str.size() >= 1) {
            str += "\n";
        }
        str += this->mStringView.to_string();
    }

};

void Value::appendStr(boost::string_view const& strView)
{
    boost::apply_visitor(AppendString(strView), this->pData->data);
}

using ValueTypeBimap = boost::bimap<boost::string_view, Value::Type>;
static const ValueTypeBimap valueTypeBimap = boost::assign::list_of<ValueTypeBimap::relation>
    ("none", Value::Type::None)
    ("bool", Value::Type::Bool)
    ("string", Value::Type::String)
    ("number", Value::Type::Number)
    ("array", Value::Type::Array)
    ("object", Value::Type::Object)
    ("objectDefined", Value::Type::ObjectDefined)
    ("memberDefined", Value::Type::MemberDefined)
    ("reference", Value::Type::Reference)
    ("function", Value::Type::Function)
    ("capture", Value::Type::Capture)
    ("coroutine", Value::Type::Coroutine);

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

    std::string operator()(bool const& b)const
    {
        return (b ? "true"s : "false"s);
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

    std::string operator()(Reference const& ref)const
    {
        std::string separater = "";
        std::string name = "";
        for (auto&& n : ref.nestName) {
            name += separater + n;
            separater = '.';
        }
        return "[Reference](" + name + ")";
    }

    std::string operator()(Function const& function)const
    {
        auto str = "[Function]("s;
        str += "args[ ";
        for (auto&& arg : function.arguments) {
            str += arg.name + ":" + Value::toString(arg.defaultValue.type).to_string() + " ";
        }
        str += "])";
        return str;
    }

    std::string operator()(Argument const& arg)const
    {
        return "[Argument](" + arg.name + ":" + Value::toString(arg.defaultValue.type).to_string() + ")";
    }

    std::string operator()(Capture const& arg)const
    {
        return "[Capture](" + arg.name + ":" + Value::toString(arg.value.type).to_string() + ")";
    }

    std::string operator()(Coroutine const& arg)const
    {
        return "[Coroutine]";
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

Value& Value::getChild(std::string const& name)
{
    auto const* constThis = this;
    return const_cast<Value&>(constThis->getChild(name));
}

Value const& Value::getChild(std::string const& name)const
{
    switch (this->type) {
    case Value::Type::Object:
    {
        auto& obj = this->get<Value::object>();
        auto it = obj.members.find(name);
        if (obj.members.end() == it) {
            AWESOME_THROW(std::invalid_argument)
                << "Don't found '" << name << "' child.";
            break;
        }
        return it->second;
    }
    case Value::Type::Array:
    {

        bool isNumber = false;
        auto index = static_cast<int>(toDouble(name, isNumber));
        if (!isNumber) {
            AWESOME_THROW(std::invalid_argument)
                << "Use index(" << index << ") outside the array range. (array size=" << this->get<Value::array>().size() << ")";
            break;
        }
        auto& arr = this->get<Value::array>();
        if (arr.size() <= index) {
            AWESOME_THROW(std::invalid_argument)
                << "syntax error!! Use index(" << index << ") outside the array range. (array size=" << this->get<Value::array>().size() << ")";
        }
        return arr[index];
    }
    default: break;
    }
    return Value::none;
}

//-----------------------------------------------------------------------
//
//  struct RefOrEntityValue
//
//-----------------------------------------------------------------------

RefOrEntityValue::RefOrEntityValue(Value const& value)
    : mData(value)
{}

RefOrEntityValue::RefOrEntityValue(Value const* pValue)
{
    if (nullptr != pValue) {
        this->mData = pValue;
    } else {
        this->mData = Value::none;
    }
}

class GetValueFromRefOrEntityValue : public boost::static_visitor<Value const&>
{
public:
    Value const& operator()(Value const& entity)const
    {
        return entity;
    }
    Value const& operator()(Value const* pValue)const
    {
        return *pValue;
    }
};

Value const& RefOrEntityValue::value()const
{
    return boost::apply_visitor(GetValueFromRefOrEntityValue(), this->mData);
}

//-----------------------------------------------------------------------
//
//  struct Argument
//
//-----------------------------------------------------------------------

Argument::Argument()
    : name()
    , type(Value::Type::None)
    , defaultValue(Value::none)
{}

}