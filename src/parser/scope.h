#pragma once

#include <string>

#include <boost/utility/string_view.hpp>

#include "value.h"
#include "parserUtility.h"

namespace parser
{

class Line;

class IScope
{
public:
    enum class Type
    {
        Normal,
        Reference,
        Boolean,
        Branch,
        Dummy,
    };

public:
    virtual ~IScope() {}

    virtual Type type()const = 0;
    virtual std::list<std::string> const& nestName()const = 0;
    virtual Value& value() = 0;
    virtual Value const& value()const = 0;
    virtual Value::Type valueType()const = 0;
};

class NormalScope : public IScope
{
    std::list<std::string> mNestName;
    Value mValue;

public:
    NormalScope()=default;
    NormalScope(std::list<std::string> const& nestName, Value const& value);
    NormalScope(std::list<std::string> && nestName, Value && value);
    NormalScope(std::list<boost::string_view> const& nestName, Value const& value);

    Type type()const override;

    std::list<std::string> const& nestName()const override;
    Value& value() override;
    Value const& value()const override;
    Value::Type valueType()const override;
};

class ReferenceScope : public IScope
{
    std::list<std::string> mNestName;
    Value& mRefValue;
    bool mDoPopModeAtCloging;
public:
    ReferenceScope(std::list<std::string> const& nestName, Value& value, bool doPopModeAtCloging);
    ReferenceScope(std::list<boost::string_view> const& nestName, Value & value, bool doPopModeAtCloging);
    Type type()const override;

    std::list<std::string> const& nestName()const override;
    Value& value() override;
    Value const& value()const override;
    Value::Type valueType()const;

    bool doPopModeAtClosing()const;
};

class BooleanScope : public IScope
{
    std::list<std::string> mNestName;
    Value mValue;
    LogicOperator mLogicOp;
    bool mDoSkip;
    int mTrueCount;
    int mFalseCount;
    bool mIsDenial;

public:
    BooleanScope(std::list<std::string> const& nestName, bool isDenial);
    BooleanScope(std::list<boost::string_view> const& nestName, bool isDenial);

    Type type()const override;
    std::list<std::string> const& nestName()const override;
    Value& value() override;
    Value const& value()const override;
    Value::Type valueType()const override;

    void setLogicOperator(LogicOperator op);
    bool doEvalValue()const;
    void tally(bool b);
    bool result()const;
};

class BranchScope : public IScope
{
    IScope& mParentScope;
    Value const* mpSwitchTargetVariable;
    //
    bool mIsDenial;
    bool mDoRunAllTrueStatement;
    int mRunningCountOfTrueStatement;
    //
    int mTrueCount;
    int mFalseCount;
    LogicOperator mLogicOp;
    bool mDoSkip;

public:
    BranchScope(IScope& parentScope, Value const* pSwitchTargetVariable, bool isDenial);

    Type type()const override;

    std::list<std::string> const& nestName()const override;
    Value& value() override;
    Value const& value()const override;
    Value::Type valueType()const;

    bool isSwitch()const;
    Value const& switchTargetValue()const;

    void tally(bool result);
    bool doCurrentStatements()const;
    void resetBranchState();

    void setLogicOperator(LogicOperator op);
    bool doEvalValue()const;

    // operate running count of true statement
    void incrementRunningCount();
    bool doElseStatement()const;
};

class DummyScope : public IScope
{
public:
    Type type()const override;

    std::list<std::string> const& nestName()const override;
    Value& value() override;
    Value const& value()const override;
    Value::Type valueType()const override;

};

}