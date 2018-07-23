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
        DefineFunction,
        CallFunction,
        CallFunctionArguments,
        CallFunctionReturnValues,
        Send,
        PassTo,
        ArrayAccessor,
    };

    static boost::string_view toString(Type type);

public:
    virtual ~IScope() {}

    virtual void close(Enviroment& env);
    Value* searchVariable(std::string const& name);
    virtual Value const* searchVariable(std::string const& name)const;

    virtual Type type()const = 0;
    virtual std::list<std::string> const& nestName()const = 0;
    Value& value();
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
    void close(Enviroment& env)override;

    std::list<std::string> const& nestName()const override;
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
    void close(Enviroment& env)override;
    std::list<std::string> const& nestName()const override;
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

    Object mLocalVariables;

public:
    BranchScope(IScope& parentScope, Value const* pSwitchTargetVariable, bool isDenial);

    Type type()const override;
    void close(Enviroment& env)override;
    Value const* searchVariable(std::string const& name)const override;

    std::list<std::string> const& nestName()const override;
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

    // operate local variable
    void addLocalVariable(std::string const& name, Value const& value);

};

class DummyScope : public IScope
{
public:
    Type type()const override;

    void close(Enviroment& env)override;
    Value const* searchVariable(std::string const& name)const override;

    std::list<std::string> const& nestName()const override;
    Value const& value()const override;
    Value::Type valueType()const override;

};

class DefineFunctionScope : public IScope
{
    IScope& mParentScope;
    DefineFunctionOperator mOp;
    std::vector<Value> mElements;

public:
    DefineFunctionScope(IScope& parentScope, DefineFunctionOperator op);

    Type type()const override;
    void close(Enviroment& env)override;
    Value const* searchVariable(std::string const& name)const override;

    std::list<std::string> const& nestName()const override;
    Value const& value()const override;
    Value::Type valueType()const;

    void addElememnt(Value const& element);
    void addElememnt(Value&& element);
    void setValueToCurrentElement(Value const& value);
    void appendContentsLine(Enviroment const& env, std::string const& line);
};

class CallFunctionScope : public IScope
{
    IScope& mParentScope;
    Value& mFunction;
    std::vector<Value> mArguments;
    std::vector<std::list<std::string>> mReturnValues;

public:
    CallFunctionScope(IScope& parentScope, Value& function);

    void close(Enviroment& env);
    Value const* searchVariable(std::string const& name)const override;

    Type type()const override;
    std::list<std::string> const& nestName()const override;
    Value const& value()const override;
    Value::Type valueType()const override;

    void setArguments(std::vector<Value>&& args);
    void setReturnValueNames(std::vector<std::list<std::string>>&& names);
    Function const& function()const;
};

class CallFunctionArgumentsScope : public IScope
{
    IScope& mParentScope;
    std::vector<Value> mArguments;

public:
    CallFunctionArgumentsScope(IScope& parentScope, size_t expectedArgumentsCount);

    void close(Enviroment& env);
    Value const* searchVariable(std::string const& name)const override;

    Type type()const override;
    std::list<std::string> const& nestName()const override;
    Value const& value()const override;
    Value::Type valueType()const override;

    void pushArgument(Value&& value);
    std::vector<Value>&& moveArguments();
};

class CallFunctionReturnValueScope : public IScope
{
    IScope& mParentScope;
    std::vector<std::list<std::string>> mReturnValues;

public:
    CallFunctionReturnValueScope(IScope& parentScope);

    void close(Enviroment& env);
    Value const* searchVariable(std::string const& name)const override;

    Type type()const override;
    std::list<std::string> const& nestName()const override;
    Value const& value()const override;
    Value::Type valueType()const override;

    void pushReturnValueName(std::list<std::string>&& nestName);
    std::vector<std::list<std::string>>&& moveReturnValueNames();
};

class SendScope : public IScope
{
    std::vector<Value> mReturnValues;
    bool mIsFinished;
public:
    SendScope(bool isFinished);

    void close(Enviroment& env)override;
    Value const* searchVariable(std::string const& name)const override;

    Type type()const override;
    std::list<std::string> const& nestName()const override;
    Value const& value()const override;
    Value::Type valueType()const override;

    void pushValue(Value const& value);
    void pushValue(Value && value);
};

class PassToScope : public IScope
{
    IScope& mParentScope;
public:
    PassToScope(IScope& parentScope);

    void close(Enviroment& env)override;
    Value const* searchVariable(std::string const& name)const override;

    Type type()const override;
    std::list<std::string> const& nestName()const override;
    Value const& value()const override;
    Value::Type valueType()const override;

    void passValue(Enviroment& env, Value const& value);
    void passValue(Enviroment& env, Value && value);
};

class ArrayAccessorScope : public IScope
{
    std::list<std::string> mNestName;
    std::vector<size_t> mIndices;
    Value mValueToPass;
    bool mIsAll;

public:
    ArrayAccessorScope(std::list<std::string> const& nestName);
    ArrayAccessorScope(std::list<boost::string_view> const& nestName);

    void close(Enviroment& env)override;
    Value const* searchVariable(std::string const& name)const override;

    Type type()const override;
    std::list<std::string> const& nestName()const override;
    Value const& value()const override;
    Value::Type valueType()const override;

    void setValueToPass(Value::array const& arr);
    void setValueToPass(Value::array && arr);

    void pushIndex(size_t index);
    bool doAccessed()const;
};

}