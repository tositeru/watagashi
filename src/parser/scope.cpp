#include "scope.h"

#include "line.h"

#include "parseMode.h"
#include "enviroment.h"

#include "mode/defineFunction.h"

namespace parser
{
//----------------------------------------------------------------------------------
//
//  class IScope
//
//----------------------------------------------------------------------------------

void IScope::close(Enviroment& env)
{
    // postprocess
    if (Value::Type::ObjectDefined == this->valueType()) {
        auto& objectDefined = this->value().get<ObjectDefined>();
        objectDefined.name = this->nestName().back();
        env.popMode();

    } else if (Value::Type::Object == this->valueType()) {
        auto& obj = this->value().get<Value::object>();
        if (!obj.applyObjectDefined()) {
            std::string fullname = toNameString(this->nestName());
            throw MakeException<DefinedObjectException>()
                << "An undefined member exists in '" << fullname << "." << MAKE_EXCEPTION;
        }

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

    } else {
        Value* pParentValue = nullptr;
        if (2 <= this->nestName().size()) {
            pParentValue = searchValue(this->nestName(), env, true);
        } else {
            pParentValue = &env.currentScope().value();
        }
        switch (pParentValue->type) {
        case Value::Type::Object:
            if (!pParentValue->addMember(*this)) {
                throw MakeException<SyntaxException>()
                    << "Failed to add an element to the current scope object." << MAKE_EXCEPTION;
            }
            break;
        case Value::Type::Array:
            if ("" == this->nestName().back()) {
                pParentValue->pushValue(this->value());
            } else {
                if (!pParentValue->addMember(*this)) {
                    throw MakeException<SyntaxException>()
                        << "Failed to add an element to the current scope array because it was a index out of range."
                        << MAKE_EXCEPTION;
                }
            }
            break;
        case Value::Type::ObjectDefined:
            if (!pParentValue->addMember(*this)) {
                throw MakeException<SyntaxException>()
                    << "Failed to add an element to the current scope object."
                    << MAKE_EXCEPTION;
            }
            break;
        case Value::Type::MemberDefined:
        {
            auto& memberDefined = pParentValue->get<MemberDefined>();
            memberDefined.defaultValue = this->value();
            break;
        }
        default:
            throw MakeException<SyntaxException>()
                << "The current value can not have children."
                << MAKE_EXCEPTION;
        }
    }
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

Value& NormalScope::value()
{
    return this->mValue;
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

Value& ReferenceScope::value()
{
    return this->mRefValue;
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
    this->value() = this->result();
    env.popMode();
    IScope::close(env);
}

std::list<std::string> const& BooleanScope::nestName()const
{
    return this->mNestName;
}

Value& BooleanScope::value()
{
    return this->mValue;
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
                << "(prev: " << toString(this->mLogicOp) << ", now:" << toString(op) << ")";
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
{}

IScope::Type BranchScope::type()const
{
    return Type::Branch;
}

void BranchScope::close(Enviroment& env)
{
    env.popMode();
}

std::list<std::string> const& BranchScope::nestName()const
{
    return this->mParentScope.nestName();
}

Value& BranchScope::value()
{
    return this->mParentScope.value();
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
                << "(prev: " << toString(this->mLogicOp) << ", now:" << toString(op) << ")";
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

std::list<std::string> const& DummyScope::nestName()const
{
    static std::list<std::string> const dummy = {};
    return dummy;
}

Value& DummyScope::value()
{
    static Value dummy(Value::none);
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

std::list<std::string> const& DefineFunctionScope::nestName()const
{
    return this->mParentScope.nestName();
}

Value& DefineFunctionScope::value()
{
    return this->mParentScope.value();
}

Value const& DefineFunctionScope::value()const
{
    return this->mParentScope.value();
}

Value::Type DefineFunctionScope::valueType()const
{
    return this->mParentScope.valueType();
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
            << "don't set value to current element at " << toString(this->mOp) << ".";
    }
}

}