#include "scope.h"

#include <string>

#include <boost/bimap.hpp>
#include <boost/assign.hpp>
#include <boost/range/irange.hpp>

#include "line.h"

#include "parseMode.h"
#include "enviroment.h"
#include "parser.h"

#include "mode/defineFunction.h"
#include "mode/send.h"

namespace parser
{
//----------------------------------------------------------------------------------
//
//  common functions
//
//----------------------------------------------------------------------------------

void addValueToParent(Enviroment& env, IScope& scope)
{
    Value* pParentValue = nullptr;
    if (2 <= scope.nestName().size()) {
        pParentValue = env.searchValue(scope.nestName(), true);
    } else {
        pParentValue = &env.currentScope().value();
    }
    switch (pParentValue->type) {
    case Value::Type::Object:
        if (!pParentValue->addMember(scope)) {
            AWESOME_THROW(SyntaxException) << "Failed to add an element to the current scope object.";
        }
        break;
    case Value::Type::Array:
        if ("" == scope.nestName().back()) {
            pParentValue->pushValue(scope.value());
        } else {
            if (!pParentValue->addMember(scope)) {
                AWESOME_THROW(SyntaxException)
                    << "Failed to add an element to the current scope array because it was a index out of range.";
            }
        }
        break;
    case Value::Type::ObjectDefined:
        if (!pParentValue->addMember(scope)) {
            AWESOME_THROW(SyntaxException)
                << "Failed to add an element to the current scope object.";
        }
        break;
    case Value::Type::MemberDefined:
    {
        auto& memberDefined = pParentValue->get<MemberDefined>();
        memberDefined.defaultValue = scope.value();
        break;
    }
    default:
        AWESOME_THROW(SyntaxException)
            << "The current value can not have children.";
    }
}

//----------------------------------------------------------------------------------
//
//  class IScope
//
//----------------------------------------------------------------------------------
using ScopeTypeBimap = boost::bimap<boost::string_view, IScope::Type>;
static ScopeTypeBimap const scopeTypeBimap = boost::assign::list_of<ScopeTypeBimap::relation>
    ("normal", IScope::Type::Normal)
    ("reference", IScope::Type::Reference)
    ("boolean", IScope::Type::Boolean)
    ("branch", IScope::Type::Branch)
    ("dummy", IScope::Type::Dummy)
    ("defineFunction", IScope::Type::DefineFunction)
    ("callFunction", IScope::Type::CallFunction)
    ("callFunctionArguments", IScope::Type::CallFunctionArguments)
    ("callFunctionReturnValues", IScope::Type::CallFunctionReturnValues);

boost::string_view IScope::toString(Type type)
{
    static char const* UNKNOWN = "(unknown)";
    auto it = scopeTypeBimap.right.find(type);
    return scopeTypeBimap.right.end() == it
        ? UNKNOWN
        : it->get_left();
}

void IScope::close(Enviroment& env)
{
    // postprocess
    if (Value::Type::ObjectDefined == this->valueType()) {
        auto& objectDefined = this->value().get<ObjectDefined>();
        objectDefined.name = this->nestName().back();
        env.popMode();

    } else if (Value::Type::Object == this->valueType()) {
        auto& obj = this->value().get<Value::object>();
        obj.applyObjectDefined();

    } else if (Value::Type::String == this->valueType()) {
        auto& str = this->value().get<Value::string>();
        str = expandVariable(str, env);
    } else if (Value::Type::Function == this->valueType()) {
        env.popMode();
    }

    // set parsing value to the parent
    if (env.currentScope().type() == IScope::Type::DefineFunction) {
        auto& defineFunctionScope = dynamic_cast<DefineFunctionScope&>(env.currentScope());
        defineFunctionScope.setValueToCurrentElement(this->value());
        env.popMode();

    } else if (env.currentScope().type() == IScope::Type::Branch) {
        auto& branchScope = dynamic_cast<BranchScope&>(env.currentScope());
        branchScope.addLocalVariable(this->nestName().back(), this->value());
        env.popMode();

    } else if(env.currentScope().type() == IScope::Type::CallFunctionArguments) {
        auto& callArgumentsScope = dynamic_cast<CallFunctionArgumentsScope&>(env.currentScope());
        callArgumentsScope.pushArgument(std::move(this->value()));
        env.popMode();

    } else if (env.currentScope().type() == IScope::Type::Send) {
        auto& returnScope = dynamic_cast<SendScope&>(env.currentScope());
        if (this->valueType() == Value::Type::Reference) {
            auto& ref = this->value().get<Reference>();
            auto pValue = ref.ref();
            returnScope.pushValue(*pValue);

        } else {
            returnScope.pushValue(this->value());
        }

        while (true) {
            if (dynamic_cast<SendParseMode*>(env.currentMode().get())) {
                break;
            }
            env.popMode();
        }

    } else {
        addValueToParent(env, *this);
    }
}

Value* IScope::searchVariable(std::string const& name) {
    auto const* constThis = this;
    return const_cast<Value*>(constThis->searchVariable(name));
}

Value const* IScope::searchVariable(std::string const& name)const
{
    if (this->nestName().back() == name) {
        return &this->value();
    }
    if (this->value().isExsitChild(name)) {
        auto& childValue = this->value().getChild(name);
        return &childValue;
    }
    return nullptr;
}

Value& IScope::value()
{
    auto constThis = const_cast<IScope const*>(this);
    return const_cast<Value&>(constThis->value());
}

//----------------------------------------------------------------------------------
//
//  class NormalScope
//
//----------------------------------------------------------------------------------
NormalScope::NormalScope(std::list<std::string> const& nestName, Value const& value)
    : mNestName(nestName)
    , mValue(value)
{}
NormalScope::NormalScope(std::list<std::string> && nestName, Value && value)
    : mNestName(std::move(nestName))
    , mValue(std::move(value))
{}
NormalScope::NormalScope(std::list<boost::string_view> const& nestName, Value const& value)
    : mNestName()
    , mValue(value)
{
    this->mNestName = toStringList(nestName);
}

IScope::Type NormalScope::type()const{
    return Type::Normal;
}

std::list<std::string> const& NormalScope::nestName()const
{
    return this->mNestName;
}

Value const& NormalScope::value()const
{
    return this->mValue;
}

Value::Type NormalScope::valueType()const
{
    return this->mValue.type;
}

//----------------------------------------------------------------------------------
//
//  class ReferenceScope
//
//----------------------------------------------------------------------------------
ReferenceScope::ReferenceScope(std::list<std::string> const& nestName, Value& value, bool doPopModeAtCloging)
    : mNestName(nestName)
    , mRefValue(value)
    , mDoPopModeAtCloging(doPopModeAtCloging)
{}
ReferenceScope::ReferenceScope(std::list<boost::string_view> const& nestName, Value & value, bool doPopModeAtCloging)
    : mNestName()
    , mRefValue(value)
    , mDoPopModeAtCloging(doPopModeAtCloging)
{
    this->mNestName = toStringList(nestName);
}

IScope::Type ReferenceScope::type()const{
    return Type::Reference;
}
void ReferenceScope::close(Enviroment& env)
{
    if (this->doPopModeAtClosing()) {
        env.popMode();
    }
}

std::list<std::string> const& ReferenceScope::nestName()const
{
    return this->mNestName;
}

Value const& ReferenceScope::value()const
{
    return this->mRefValue;
}

Value::Type ReferenceScope::valueType()const
{
    return this->value().type;
}

bool ReferenceScope::doPopModeAtClosing()const
{
    return this->mDoPopModeAtCloging;
}

//----------------------------------------------------------------------------------
//
//  class BooleanScope
//
//----------------------------------------------------------------------------------
BooleanScope::BooleanScope(std::list<std::string> const& nestName, bool isDenial)
    : mNestName(nestName)
    , mValue(false)
    , mLogicOp(LogicOperator::Continue)
    , mDoSkip(false)
    , mTrueCount(0)
    , mFalseCount(0)
    , mIsDenial(isDenial)
{}

BooleanScope::BooleanScope(std::list<boost::string_view> const& nestName, bool isDenial)
    : mNestName()
    , mValue(false)
    , mLogicOp(LogicOperator::Continue)
    , mDoSkip(false)
    , mTrueCount(0)
    , mFalseCount(0)
    , mIsDenial(isDenial)
{
    this->mNestName = toStringList(nestName);
}

IScope::Type BooleanScope::type()const {
    return Type::Boolean;
}

void BooleanScope::close(Enviroment& env)
{
    IScope::value() = this->result();
    env.popMode();
    IScope::close(env);
}

std::list<std::string> const& BooleanScope::nestName()const
{
    return this->mNestName;
}

Value const& BooleanScope::value()const
{
    return this->mValue;
}

Value::Type BooleanScope::valueType()const
{
    return this->value().type;
}

void BooleanScope::setLogicOperator(LogicOperator op)
{
    if (LogicOperator::Unknown == op) {
        AWESOME_THROW(std::invalid_argument) << "unknown logical operators...";
    }

    if (LogicOperator::Continue == op) {
        return;
    }

    switch (this->mLogicOp) {
    case LogicOperator::And: [[fallthrough]];
    case LogicOperator::Or:
        if (this->mLogicOp != op) {
            AWESOME_THROW(SyntaxException)
                << "can not specify different logical operators..."
                << "(prev: " << parser::toString(this->mLogicOp) << ", now:" << parser::toString(op) << ")";
        }
        break;
    default:
        break;
    }

    this->mLogicOp = op;
    switch (op) {
    case LogicOperator::And:
        if (1 <= this->mFalseCount) {
            this->mDoSkip = true;
        }
    case LogicOperator::Or:
        if (1 <= this->mTrueCount) {
            this->mDoSkip = true;
        }
    default:
        break;
    }
}

bool BooleanScope::doEvalValue()const
{
    return !this->mDoSkip;
}

void BooleanScope::tally(bool b)
{
    if (b) {
        ++this->mTrueCount;
        if (LogicOperator::Or == this->mLogicOp) {
            this->mDoSkip = true;
        }
    } else {
        ++this->mFalseCount;
        if (LogicOperator::And == this->mLogicOp) {
            this->mDoSkip = true;
        }
    }
}

bool BooleanScope::result()const
{
    bool result = false;
    switch(this->mLogicOp) {
    case LogicOperator::Or:
        result = 1 <= this->mTrueCount; break;
    case LogicOperator::Continue: [[fallthrough]];
    case LogicOperator::And:
        result = this->mFalseCount <= 0; break;
    default:
        AWESOME_THROW(BooleanException)
            << "use unknown logic operator...";
    }
    return this->mIsDenial ? !result : result;
}

//----------------------------------------------------------------------------------
//
//  class BranchScope
//
//----------------------------------------------------------------------------------
BranchScope::BranchScope(IScope& parentScope, Value const* pSwitchTargetVariable, bool isDenial)
    : mParentScope(parentScope)
    , mpSwitchTargetVariable(pSwitchTargetVariable)
    , mIsDenial(isDenial)
    , mDoRunAllTrueStatement(false)
    , mRunningCountOfTrueStatement(0)
    , mTrueCount(0)
    , mFalseCount(0)
    , mLogicOp(LogicOperator::Continue)
    , mDoSkip(false)
    , mLocalVariables(&Value::emptyObjectDefined)
{}

IScope::Type BranchScope::type()const
{
    return Type::Branch;
}

void BranchScope::close(Enviroment& env)
{
    env.popMode();
}

Value const* BranchScope::searchVariable(std::string const& name)const
{
    if (!this->mLocalVariables.isExistMember(name)) {
        return nullptr;
    }
    return &this->mLocalVariables.getMember(name);
}

std::list<std::string> const& BranchScope::nestName()const
{
    return this->mParentScope.nestName();
}

Value const& BranchScope::value()const
{
    return this->mParentScope.value();
}

Value::Type BranchScope::valueType()const
{
    return this->mParentScope.valueType();
}

bool BranchScope::isSwitch()const
{
    return nullptr != this->mpSwitchTargetVariable;
}

Value const& BranchScope::switchTargetValue()const
{
    if (!this->isSwitch()) {
        AWESOME_THROW(std::runtime_error)
            << "BranchScope do not be set the switch target...";
    }
    return *this->mpSwitchTargetVariable;
}

void BranchScope::tally(bool result)
{
    if (result) {
        ++this->mTrueCount;
        if (LogicOperator::Or == this->mLogicOp) {
            this->mDoSkip = true;
        }
    } else {
        ++this->mFalseCount;
        if (LogicOperator::And == this->mLogicOp) {
            this->mDoSkip = true;
        }
    }

}

bool BranchScope::doCurrentStatements()const
{
    if (!this->mDoRunAllTrueStatement && 1 <= this->mRunningCountOfTrueStatement) {
        return false;
    }

    bool result = false;
    switch (this->mLogicOp) {
    case LogicOperator::Or:
        result = 1 <= this->mTrueCount; break;
    case LogicOperator::Continue: [[fallthrough]];
    case LogicOperator::And:
        result = this->mFalseCount <= 0; break;
    default:
        AWESOME_THROW(BooleanException)
            << "use unknown logic operator...";
    }
    return this->mIsDenial ? !result : result;
}

void BranchScope::resetBranchState()
{
    this->mTrueCount = 0;
    this->mFalseCount = 0;
    this->mLogicOp = LogicOperator::Continue;
    this->mDoSkip = false;
}

void BranchScope::setLogicOperator(LogicOperator op)
{
    if (LogicOperator::Unknown == op) {
        AWESOME_THROW(std::invalid_argument) << "unknown logical operators...";
    }

    if (LogicOperator::Continue == op) {
        return;
    }

    switch (this->mLogicOp) {
    case LogicOperator::And: [[fallthrough]];
    case LogicOperator::Or:
        if (this->mLogicOp != op) {
            AWESOME_THROW(SyntaxException)
                << "can not specify different logical operators..."
                << "(prev: " << parser::toString(this->mLogicOp) << ", now:" << parser::toString(op) << ")";
        }
        break;
    default:
        break;
    }

    this->mLogicOp = op;
    switch (op) {
    case LogicOperator::And:
        if (1 <= this->mFalseCount) {
            this->mDoSkip = true;
        }
    case LogicOperator::Or:
        if (1 <= this->mTrueCount) {
            this->mDoSkip = true;
        }
    default:
        break;
    }
}

bool BranchScope::doEvalValue()const
{
    return !this->mDoSkip;
}

void BranchScope::incrementRunningCount()
{
    ++this->mRunningCountOfTrueStatement;
}

bool BranchScope::doElseStatement()const
{
    return this->mRunningCountOfTrueStatement <= 0;
}

void BranchScope::addLocalVariable(std::string const& name, Value const& value)
{
    this->mLocalVariables.members.insert({name, value});
}

//----------------------------------------------------------------------------------
//
//  class DummyScope
//
//----------------------------------------------------------------------------------
IScope::Type DummyScope::type()const
{
    return Type::Dummy;
}

void DummyScope::close(Enviroment& env)
{
    env.popMode();
}

Value const* DummyScope::searchVariable(std::string const& /*name*/)const
{
    return nullptr;
}

std::list<std::string> const& DummyScope::nestName()const
{
    static std::list<std::string> const dummy = {""};
    return dummy;
}

Value const& DummyScope::value()const
{
    return Value::none;
}

Value::Type DummyScope::valueType()const
{
    return Value::Type::None;
}

//----------------------------------------------------------------------------------
//
//  class DefineFunctionScope
//
//----------------------------------------------------------------------------------
DefineFunctionScope::DefineFunctionScope(IScope& parentScope, DefineFunctionOperator op)
    : mParentScope(parentScope)
    , mOp(op)
{
    if (this->mParentScope.valueType() != Value::Type::Function) {
        AWESOME_THROW(FatalException) << "The value type in parent scope is not Function...";
    }
}

IScope::Type DefineFunctionScope::type()const
{
    return Type::DefineFunction;
}

void DefineFunctionScope::close(Enviroment& env)
{
    if (Value::Type::Function != this->mParentScope.valueType()) {
        AWESOME_THROW(FatalException)
            << "The type of value in parent scope of DefineFunctionScope must be Function...";
    }

    auto& function = this->mParentScope.value().get<Function>();
    switch (this->mOp) {
    case DefineFunctionOperator::ToPass:
        function.arguments.reserve(this->mElements.size());
        for (auto&& e : this->mElements) {
            function.arguments.emplace_back(e.get<Value::argument>());
        }
        break;
    case DefineFunctionOperator::ToCapture:
        function.captures.reserve(this->mElements.size());
        for (auto&& e : this->mElements) {
            function.captures.emplace_back(e.get<Value::capture>());
        }
        break;
    case DefineFunctionOperator::WithContents:
    {
        size_t contentLength = 0u;
        for (auto&& e : this->mElements) {
            contentLength += e.get<Value::string>().size();
        }
        function.contents.clear();
        function.contents.reserve(contentLength);
        for (auto&& e : this->mElements) {
            function.contents += e.get<Value::string>();
            function.contents += "\n";
        }
        break;
    }
    default:
        AWESOME_THROW(std::runtime_error)
            << "use unknown DefineFunctionOperator in close()...";
    }

    if (auto pDefineFunctionParser = dynamic_cast<DefineFunctionParseMode*>(env.currentMode().get())) {
        pDefineFunctionParser->resetMode();
    }
}

Value const* DefineFunctionScope::searchVariable(std::string const& /*name*/)const
{
    return nullptr;
}

std::list<std::string> const& DefineFunctionScope::nestName()const
{
    return this->mParentScope.nestName();
}

Value const& DefineFunctionScope::value()const
{
    return this->mParentScope.value();
}

Value::Type DefineFunctionScope::valueType()const
{
    return this->mParentScope.valueType();
}

void DefineFunctionScope::addElememnt(Value const& element)
{
    this->mElements.push_back(element);
}

void DefineFunctionScope::addElememnt(Value&& element)
{
    this->mElements.push_back(std::move(element));
}

void DefineFunctionScope::setValueToCurrentElement(Value const& value)
{
    auto& element = this->mElements.back();
    switch (this->mOp) {
    case DefineFunctionOperator::ToPass:
    {
        auto& arg = element.get<Argument>();
        arg.defaultValue = value;
        break;
    }
    default:
        AWESOME_THROW(FatalException)
            << "don't set value to current element at " << parser::toString(this->mOp) << ".";
    }
}

void DefineFunctionScope::appendContentsLine(Enviroment const& env, std::string const& line)
{
    if (this->mElements.empty()) {
        auto& func = this->mParentScope.value().get<Value::function>();
        func.contentsLocation = env.location;
        func.contentsLocation.row = env.source.row()-1;
    }
    this->addElememnt(Value(line));
}

//----------------------------------------------------------------------------------
//
//  class CallFunctionScope
//
//----------------------------------------------------------------------------------
CallFunctionScope::CallFunctionScope(IScope& parentScope, Function const& function)
    : mParentScope(parentScope)
    , mFunction(function)
{
    this->mArguments.reserve(function.arguments.size());
}

void CallFunctionScope::close(Enviroment& env)
{
    ParserDesc parseDesc;
    parseDesc.location = this->mFunction.contentsLocation;
    for (auto&& capture : this->mFunction.captures) {
        parseDesc.externObj.addMember(capture.name, capture.value);
    }
    // setup arguments
    for (auto index : boost::irange(size_t(0), this->mArguments.size())) {
        auto& formalArgument = this->mFunction.arguments[index];
        auto& entity = this->mArguments[index];
        parseDesc.globalObj.addMember(formalArgument.name, entity);
    }
    // setup arguments by default value
    for (auto index : boost::irange(this->mArguments.size(), this->mFunction.arguments.size())) {
        auto& formalArgument = this->mFunction.arguments[index];
        if (formalArgument.defaultValue.type == Value::Type::None) {
            AWESOME_THROW(SyntaxException)
                << "Arguments of required number not was passed.";
        }
        parseDesc.globalObj.addMember(formalArgument.name, formalArgument.defaultValue);
    }
    
    auto result = parse(this->mFunction.contents, parseDesc);

    auto const returnValueCount = std::min(this->mReturnValues.size(), result.returnValues.size());
    for(auto returnValueIndex : boost::irange(size_t(0), returnValueCount)) {
        auto& nestName = this->mReturnValues[returnValueIndex];
        Value* pParentValue = nullptr;
        if (2 <= this->nestName().size()) {
            pParentValue = env.searchValue(this->nestName(), true);
        } else {
            pParentValue = &this->mParentScope.value();
        }
        if (!pParentValue->addMember(nestName.back(), result.returnValues[returnValueIndex]) ) {
            AWESOME_THROW(FatalException)
                << "Failed to set a return value from function. name=" << toNameString(nestName);
        }
    }

    if (env.currentScope().type() == IScope::Type::ArrayAccessor) {
        auto& arrayAccessorScope = dynamic_cast<ArrayAccessorScope&>(env.currentScope());
        arrayAccessorScope.setValueToPass(result.returnValues);
    }
    env.popMode();
}

Value const* CallFunctionScope::searchVariable(std::string const& /*name*/)const
{
    return nullptr;
}

IScope::Type CallFunctionScope::type()const
{
    return Type::CallFunction;
}

std::list<std::string> const& CallFunctionScope::nestName()const
{
    return this->mParentScope.nestName();
}

Value const& CallFunctionScope::value()const
{
    return this->mParentScope.value();
}

Value::Type CallFunctionScope::valueType()const
{
    return this->mParentScope.valueType();
}

void CallFunctionScope::setArguments(std::vector<Value>&& args)
{
    this->mArguments = std::move(args);
}

void CallFunctionScope::setReturnValueNames(std::vector<std::list<std::string>>&& names)
{
    this->mReturnValues = std::move(names);
}

Function const& CallFunctionScope::function()const
{
    return this->mFunction;
}

//----------------------------------------------------------------------------------
//
//  class CallFunctionArgumentsScope
//
//----------------------------------------------------------------------------------

CallFunctionArgumentsScope::CallFunctionArgumentsScope(CallFunctionScope& parentScope, size_t expectedArgumentsCount)
    : mParentScope(parentScope)
{
    this->mArguments.reserve(expectedArgumentsCount);
}

void CallFunctionArgumentsScope::close(Enviroment& env)
{
    switch (env.currentScope().type()) {
    case IScope::Type::CallFunction:
    {
        auto& parentScope = dynamic_cast<CallFunctionScope&>(env.currentScope());
        parentScope.setArguments(this->moveArguments());
        break;
    }
    default:
        AWESOME_THROW(FatalException)
            << "Unimplement to close in the case the parent scope is '" << IScope::toString(env.currentScope().type()) << "'";
    }
}

Value const* CallFunctionArgumentsScope::searchVariable(std::string const& /*name*/)const
{
    return nullptr;
}

IScope::Type CallFunctionArgumentsScope::type()const
{
    return Type::CallFunctionArguments;
}

std::list<std::string> const& CallFunctionArgumentsScope::nestName()const
{
    return this->mParentScope.nestName();
}

Value const& CallFunctionArgumentsScope::value()const
{
    return this->mParentScope.value();
}

Value::Type CallFunctionArgumentsScope::valueType()const
{
    return this->mParentScope.valueType();
}

void CallFunctionArgumentsScope::pushArgument(Value&& value)
{
    this->mArguments.push_back(std::move(value));
}

std::vector<Value>&& CallFunctionArgumentsScope::moveArguments()
{
    return std::move(this->mArguments);
}

//----------------------------------------------------------------------------------
//
//  class CallFunctionReturnValueScope
//
//----------------------------------------------------------------------------------

CallFunctionReturnValueScope::CallFunctionReturnValueScope(CallFunctionScope& parentScope)
    : mParentScope(parentScope)
{}

void CallFunctionReturnValueScope::close(Enviroment& env)
{
    switch (env.currentScope().type()) {
    case IScope::Type::CallFunction:
    {
        auto& parentScope = dynamic_cast<CallFunctionScope&>(env.currentScope());
        parentScope.setReturnValueNames(this->moveReturnValueNames());
        break;
    }
    default:
        AWESOME_THROW(FatalException)
            << "Unimplement to close in the case the parent scope is '" << IScope::toString(env.currentScope().type()) << "'";
    }
}

Value const* CallFunctionReturnValueScope::searchVariable(std::string const& /*name*/)const
{
    return nullptr;
}

IScope::Type CallFunctionReturnValueScope::type()const
{
    return Type::CallFunctionReturnValues;
}

std::list<std::string> const& CallFunctionReturnValueScope::nestName()const
{
    return this->mParentScope.nestName();
}

Value const& CallFunctionReturnValueScope::value()const
{
    return this->mParentScope.value();
}

Value::Type CallFunctionReturnValueScope::valueType()const
{
    return this->mParentScope.valueType();
}

void CallFunctionReturnValueScope::pushReturnValueName(std::list<std::string>&& nestName)
{
    this->mReturnValues.push_back(std::move(nestName));
}

std::vector<std::list<std::string>>&& CallFunctionReturnValueScope::moveReturnValueNames()
{
    return std::move(this->mReturnValues);
}

//----------------------------------------------------------------------------------
//
//  class SendScope
//
//----------------------------------------------------------------------------------
SendScope::SendScope()
{}

void SendScope::close(Enviroment& env)
{
    env.returnValues = std::move(this->mReturnValues);
    env.popMode();
}

Value const* SendScope::searchVariable(std::string const& /*name*/)const
{
    return nullptr;
}

IScope::Type SendScope::type()const
{
    return Type::Send;
}

std::list<std::string> const& SendScope::nestName()const
{
    static std::list<std::string> const dummy = {""};
    return dummy;
}

Value const& SendScope::value()const
{
    return Value::none;
}

Value::Type SendScope::valueType()const
{
    return Value::Type::None;
}

void SendScope::pushValue(Value const& value)
{
    this->mReturnValues.push_back(value);
}

void SendScope::pushValue(Value && value)
{
    this->mReturnValues.push_back(std::move(value));
}

//----------------------------------------------------------------------------------
//
//  class ArrayAccessorScope
//
//----------------------------------------------------------------------------------
ArrayAccessorScope::ArrayAccessorScope(std::list<std::string> const& nestName)
    : mNestName(nestName)
    , mIsAll(false)
{}

ArrayAccessorScope::ArrayAccessorScope(std::list<boost::string_view> const& nestName)
    : ArrayAccessorScope(toStringList(nestName))
{}

void ArrayAccessorScope::close(Enviroment& env)
{
    addValueToParent(env, *this);
    env.popMode();
}

Value const* ArrayAccessorScope::searchVariable(std::string const& /*name*/)const
{
    return nullptr;
}

IScope::Type ArrayAccessorScope::type()const
{
    return Type::ArrayAccessor;
}

std::list<std::string> const& ArrayAccessorScope::nestName()const
{
    return this->mNestName;
}

Value const& ArrayAccessorScope::value()const
{
    return this->mValueToPass;
}

Value::Type ArrayAccessorScope::valueType()const
{
    return this->mValueToPass.type;
}

void ArrayAccessorScope::setValueToPass(Value::array const& arr)
{
    if (this->mIsAll) {
        this->mValueToPass = arr;
        return;
    }
    Value::array passValues;
    passValues.reserve(this->mIndices.size());
    for (auto&& index : this->mIndices) {
        if (index >= arr.size()) {
            AWESOME_THROW(ArrayAccessException)
                << "out of range in array... index=" << index << ", array size=" <<arr.size();
        }
        passValues.push_back(arr[index]);
    }

    if (passValues.size() == 1) {
        this->mValueToPass = std::move(passValues[0]);
    } else {
        this->mValueToPass = std::move(passValues);
    }
}

void ArrayAccessorScope::setValueToPass(Value::array && arr)
{
    if (this->mIsAll) {
        this->mValueToPass = std::move(arr);
        return;
    }
    Value::array passValues;
    passValues.reserve(this->mIndices.size());
    for (auto&& index : this->mIndices) {
        if (index >= arr.size()) {
            AWESOME_THROW(ArrayAccessException)
                << "out of range in array... index=" << index << ", array size=" << arr.size();
        }
        passValues.push_back(std::move(arr[index]));
    }

    if (passValues.size() == 1) {
        this->mValueToPass = std::move(passValues[0]);
    } else {
        this->mValueToPass = std::move(passValues);
    }
    arr.clear();
}

void ArrayAccessorScope::pushIndex(size_t index)
{
    if (this->mIsAll) {
        return;
    }

    if (index == 0) {
        this->mIndices.clear();
        this->mIsAll = true;
        return;
    }

    this->mIndices.push_back(index-1);
}

bool ArrayAccessorScope::doAccessed()const
{
    return this->mValueToPass.type != Value::Type::None;
}

}