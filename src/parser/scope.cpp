#include "scope.h"

#include "line.h"

namespace parser
{
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

}