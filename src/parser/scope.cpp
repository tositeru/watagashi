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
ReferenceScope::ReferenceScope(std::list<std::string> const& nestName, Value& value)
    : mNestName(nestName)
    , mRefValue(value)
{}
ReferenceScope::ReferenceScope(std::list<boost::string_view> const& nestName, Value & value)
    : mNestName()
    , mRefValue(value)
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

}